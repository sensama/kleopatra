/*  crypto/gui/certificatelineedit.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2021, 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "certificatelineedit.h"

#include "commands/detailscommand.h"
#include "dialogs/groupdetailsdialog.h"

#include <QCompleter>
#include <QPushButton>
#include <QAction>
#include <QSignalBlocker>

#include "kleopatra_debug.h"

#include <Libkleo/KeyCache>
#include <Libkleo/KeyFilter>
#include <Libkleo/KeyGroup>
#include <Libkleo/KeyList>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeyListSortFilterProxyModel>
#include <Libkleo/Formatting>

#include <KLocalizedString>

#include <gpgme++/key.h>

#include <QGpgME/KeyListJob>
#include <QGpgME/Protocol>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>

using namespace Kleo;
using namespace GpgME;

Q_DECLARE_METATYPE(GpgME::Key)
Q_DECLARE_METATYPE(KeyGroup)

static QStringList s_lookedUpKeys;

namespace
{
class CompletionProxyModel : public KeyListSortFilterProxyModel
{
    Q_OBJECT

public:
    CompletionProxyModel(QObject *parent = nullptr)
        : KeyListSortFilterProxyModel(parent)
    {
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        Q_UNUSED(parent)
        // pretend that there is only one column to workaround a bug in
        // QAccessibleTable which provides the accessibility interface for the
        // completion pop-up
        return 1;
    }

    QVariant data(const QModelIndex &idx, int role) const override
    {
        if (!idx.isValid()) {
            return QVariant();
        }

        switch (role) {
        case Qt::DecorationRole: {
            const auto key = KeyListSortFilterProxyModel::data(idx, KeyList::KeyRole).value<GpgME::Key>();
            if (!key.isNull()) {
                return Kleo::Formatting::iconForUid(key.userID(0));
            }

            const auto group = KeyListSortFilterProxyModel::data(idx, KeyList::GroupRole).value<KeyGroup>();
            if (!group.isNull()) {
                return QIcon::fromTheme(QStringLiteral("group"));
            }

            Q_ASSERT(!key.isNull() || !group.isNull());
            return QVariant();
        }
        default:
            return KeyListSortFilterProxyModel::data(index(idx.row(), KeyList::Summary), role);
        }
    }
};

auto createSeparatorAction(QObject *parent)
{
    auto action = new QAction{parent};
    action->setSeparator(true);
    return action;
}
} // namespace

class CertificateLineEdit::Private
{
    CertificateLineEdit *q;

public:
    enum class Status
    {
        Empty,      //< text is empty
        Success,    //< a certificate or group is set
        None,       //< entered text does not match any certificates or groups
        Ambiguous,  //< entered text matches multiple certificates or groups
    };

    explicit Private(CertificateLineEdit *qq, AbstractKeyListModel *model, KeyFilter *filter);

    QString text() const;

    void setKey(const GpgME::Key &key);

    void setGroup(const KeyGroup &group);

    void setKeyFilter(const std::shared_ptr<KeyFilter> &filter);

    void updateKey();
    void editChanged();
    void editFinished();
    void checkLocate();

private:
    void openDetailsDialog();
    void setTextWithBlockedSignals(const QString &s);
    void showContextMenu(const QPoint &pos);

public:
    Status mStatus = Status::Empty;
    GpgME::Key mKey;
    KeyGroup mGroup;

    struct Ui {
        Ui(QWidget *parent)
            : lineEdit{parent}
            , button{parent}
        {}

        QLineEdit lineEdit;
        QToolButton button;
    } ui;

private:
    KeyListSortFilterProxyModel *const mFilterModel;
    KeyListSortFilterProxyModel *const mCompleterFilterModel;
    QCompleter *mCompleter = nullptr;
    std::shared_ptr<KeyFilter> mFilter;
    bool mEditStarted = false;
    bool mEditFinished = false;
    QAction *const mStatusAction;
    QAction *const mShowDetailsAction;
};

CertificateLineEdit::Private::Private(CertificateLineEdit *qq, AbstractKeyListModel *model, KeyFilter *filter)
    : q{qq}
    , ui{qq}
    , mFilterModel{new KeyListSortFilterProxyModel{qq}}
    , mCompleterFilterModel{new CompletionProxyModel{qq}}
    , mCompleter{new QCompleter{qq}}
    , mFilter{std::shared_ptr<KeyFilter>{filter}}
    , mStatusAction{new QAction{qq}}
    , mShowDetailsAction{new QAction{qq}}
{
    ui.lineEdit.setPlaceholderText(i18n("Please enter a name or email address..."));
    ui.lineEdit.setClearButtonEnabled(true);
    ui.lineEdit.setContextMenuPolicy(Qt::CustomContextMenu);
    ui.lineEdit.addAction(mStatusAction, QLineEdit::LeadingPosition);

    mCompleterFilterModel->setKeyFilter(mFilter);
    mCompleterFilterModel->setSourceModel(model);
    mCompleter->setModel(mCompleterFilterModel);
    mCompleter->setFilterMode(Qt::MatchContains);
    mCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    ui.lineEdit.setCompleter(mCompleter);

    ui.button.setIcon(QIcon::fromTheme(QStringLiteral("resource-group-new")));
    ui.button.setToolTip(i18n("Show certificate list"));
    ui.button.setAccessibleName(i18n("Show certificate list"));

    auto l = new QHBoxLayout{q};
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(&ui.lineEdit);
    l->addWidget(&ui.button);

    q->setFocusPolicy(ui.lineEdit.focusPolicy());
    q->setFocusProxy(&ui.lineEdit);

    mShowDetailsAction->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
    mShowDetailsAction->setText(i18nc("@action:inmenu", "Show Details"));
    mShowDetailsAction->setEnabled(false);

    mFilterModel->setSourceModel(model);
    mFilterModel->setFilterKeyColumn(KeyList::Summary);
    if (filter) {
        mFilterModel->setKeyFilter(mFilter);
    }

    connect(KeyCache::instance().get(), &Kleo::KeyCache::keyListingDone,
            q, [this]() { updateKey(); });
    connect(KeyCache::instance().get(), &Kleo::KeyCache::groupUpdated,
            q, [this](const KeyGroup &group) {
                if (!mGroup.isNull() && mGroup.source() == group.source() && mGroup.id() == group.id()) {
                    setTextWithBlockedSignals(Formatting::summaryLine(group));
                    // queue the update to ensure that the model has been updated
                    QMetaObject::invokeMethod(q, [this]() { updateKey(); }, Qt::QueuedConnection);
                }
            });
    connect(KeyCache::instance().get(), &Kleo::KeyCache::groupRemoved,
            q, [this](const KeyGroup &group) {
                if (!mGroup.isNull() && mGroup.source() == group.source() && mGroup.id() == group.id()) {
                    mGroup = KeyGroup();
                    QSignalBlocker blocky{&ui.lineEdit};
                    ui.lineEdit.clear();
                    // queue the update to ensure that the model has been updated
                    QMetaObject::invokeMethod(q, [this]() { updateKey(); }, Qt::QueuedConnection);
                }
            });
    connect(&ui.lineEdit, &QLineEdit::editingFinished,
            q, [this]() {
                // queue the call of editFinished() to ensure that QCompleter::activated is handled first
                QMetaObject::invokeMethod(q, [this]() { editFinished(); }, Qt::QueuedConnection);
            });
    connect(&ui.lineEdit, &QLineEdit::textChanged,
            q, [this]() { editChanged(); });
    connect(&ui.lineEdit, &QLineEdit::customContextMenuRequested,
            q, [this](const QPoint &pos) { showContextMenu(pos); });
    connect(mStatusAction, &QAction::triggered,
            q, [this]() { openDetailsDialog(); });
    connect(mShowDetailsAction, &QAction::triggered,
            q, [this]() { openDetailsDialog(); });
    connect(&ui.button, &QToolButton::clicked,
            q, &CertificateLineEdit::certificateSelectionRequested);
    connect(mCompleter, qOverload<const QModelIndex &>(&QCompleter::activated),
            q, [this] (const QModelIndex &index) {
                Key key = mCompleter->completionModel()->data(index, KeyList::KeyRole).value<Key>();
                auto group = mCompleter->completionModel()->data(index, KeyList::GroupRole).value<KeyGroup>();
                if (!key.isNull()) {
                    q->setKey(key);
                } else if (!group.isNull()) {
                    q->setGroup(group);
                } else {
                    qCDebug(KLEOPATRA_LOG) << "Activated item is neither key nor group";
                }
            });
    updateKey();
}

void CertificateLineEdit::Private::openDetailsDialog()
{
    if (!q->key().isNull()) {
        auto cmd = new Commands::DetailsCommand{q->key(), nullptr};
        cmd->setParentWidget(q);
        cmd->start();
    } else if (!q->group().isNull()) {
        auto dlg = new Dialogs::GroupDetailsDialog{q};
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setGroup(q->group());
        dlg->show();
    }
}

void CertificateLineEdit::Private::setTextWithBlockedSignals(const QString &s)
{
    QSignalBlocker blocky{&ui.lineEdit};
    ui.lineEdit.setText(s);
}

void CertificateLineEdit::Private::showContextMenu(const QPoint &pos)
{
    if (QMenu *menu = ui.lineEdit.createStandardContextMenu()) {
        auto *const firstStandardAction = menu->actions().value(0);
        menu->insertActions(firstStandardAction,
                            {mShowDetailsAction, createSeparatorAction(menu)});
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(ui.lineEdit.mapToGlobal(pos));
    }
}

CertificateLineEdit::CertificateLineEdit(AbstractKeyListModel *model,
                                         KeyFilter *filter,
                                         QWidget *parent)
    : QWidget{parent}
    , d{new Private{this, model, filter}}
{
    /* Take ownership of the model to prevent double deletion when the
     * filter models are deleted */
    model->setParent(parent ? parent : this);
}

CertificateLineEdit::~CertificateLineEdit() = default;

void CertificateLineEdit::Private::editChanged()
{
    mEditFinished = false;
    updateKey();
    if (!mEditStarted) {
        Q_EMIT q->editingStarted();
        mEditStarted = true;
    }
}

void CertificateLineEdit::Private::editFinished()
{
    mEditStarted = false;
    mEditFinished = true;
    updateKey();
    if (!q->key().isNull()) {
        setTextWithBlockedSignals(Formatting::summaryLine(q->key()));
    } else if (!q->group().isNull()) {
        setTextWithBlockedSignals(Formatting::summaryLine(q->group()));
    } else if (mStatus == Status::None) {
        checkLocate();
    }
}

void CertificateLineEdit::Private::checkLocate()
{
    if (mStatus != Status::None) {
        // try to locate key only if text matches no local certificates or groups
        return;
    }

    // Only check once per mailbox
    const auto mailText = ui.lineEdit.text().trimmed();
    if (mailText.isEmpty() || s_lookedUpKeys.contains(mailText)) {
        return;
    }
    s_lookedUpKeys << mailText;
    qCDebug(KLEOPATRA_LOG) << "Lookup job for" << mailText;
    auto job = QGpgME::openpgp()->locateKeysJob();
    job->start({mailText}, /*secretOnly=*/false);
}

void CertificateLineEdit::Private::updateKey()
{
    static const _detail::ByFingerprint<std::equal_to> keysHaveSameFingerprint;

    const auto mailText = ui.lineEdit.text();
    auto newKey = Key();
    auto newGroup = KeyGroup();
    if (mailText.isEmpty()) {
        mStatus = Status::Empty;
        mStatusAction->setIcon(QIcon::fromTheme(QStringLiteral("emblem-unavailable")));
        mStatusAction->setToolTip({});
        ui.lineEdit.setToolTip({});
    } else {
        mFilterModel->setFilterFixedString(mailText);
        if (mFilterModel->rowCount() > 1) {
            // keep current key or group if they still match
            if (!mKey.isNull()) {
                for (int row = 0; row < mFilterModel->rowCount(); ++row) {
                    const QModelIndex index = mFilterModel->index(row, 0);
                    Key key = mFilterModel->key(index);
                    if (!key.isNull() && keysHaveSameFingerprint(key, mKey)) {
                        newKey = mKey;
                        break;
                    }
                }
            } else if (!mGroup.isNull()) {
                newGroup = mGroup;
                for (int row = 0; row < mFilterModel->rowCount(); ++row) {
                    const QModelIndex index = mFilterModel->index(row, 0);
                    KeyGroup group = mFilterModel->group(index);
                    if (!group.isNull() && group.source() == mGroup.source() && group.id() == mGroup.id()) {
                        newGroup = mGroup;
                        break;
                    }
                }
            }
            if (newKey.isNull() && newGroup.isNull()) {
                mStatus = Status::Ambiguous;
                mStatusAction->setIcon(QIcon::fromTheme(QStringLiteral("emblem-question")));
                mStatusAction->setToolTip(i18n("Multiple matching certificates or groups found"));
                ui.lineEdit.setToolTip(i18n("Multiple matching certificates or groups found"));
            }
        } else if (mFilterModel->rowCount() == 1) {
            const auto index = mFilterModel->index(0, 0);
            newKey = mFilterModel->data(index, KeyList::KeyRole).value<Key>();
            newGroup = mFilterModel->data(index, KeyList::GroupRole).value<KeyGroup>();
            Q_ASSERT(!newKey.isNull() || !newGroup.isNull());
            if (newKey.isNull() && newGroup.isNull()) {
                mStatus = Status::None;
                mStatusAction->setIcon(QIcon::fromTheme(QStringLiteral("emblem-error")));
                mStatusAction->setToolTip(i18n("No matching certificates or groups found"));
                ui.lineEdit.setToolTip(i18n("No matching certificates or groups found"));
            }
        } else {
            mStatus = Status::None;
            mStatusAction->setIcon(QIcon::fromTheme(QStringLiteral("emblem-error")));
            mStatusAction->setToolTip(i18n("No matching certificates or groups found"));
            ui.lineEdit.setToolTip(i18n("No matching certificates or groups found"));
        }
    }
    mKey = newKey;
    mGroup = newGroup;

    if (!mKey.isNull()) {
        /* FIXME: This needs to be solved by a multiple UID supporting model */
        mStatus = Status::Success;
        mStatusAction->setIcon(Formatting::iconForUid(mKey.userID(0)));
        mStatusAction->setToolTip(Formatting::validity(mKey.userID(0)));
        ui.lineEdit.setToolTip(Formatting::toolTip(mKey, Formatting::ToolTipOption::AllOptions));
    } else if (!mGroup.isNull()) {
        mStatus = Status::Success;
        mStatusAction->setIcon(Formatting::validityIcon(mGroup));
        mStatusAction->setToolTip(Formatting::validity(mGroup));
        ui.lineEdit.setToolTip(Formatting::toolTip(mGroup, Formatting::ToolTipOption::AllOptions));
    }

    mShowDetailsAction->setEnabled(mStatus == Status::Success);

    Q_EMIT q->keyChanged();

    if (mailText.isEmpty()) {
        Q_EMIT q->wantsRemoval(q);
    }
}

Key CertificateLineEdit::key() const
{
    if (isEnabled()) {
        return d->mKey;
    } else {
        return Key();
    }
}

KeyGroup CertificateLineEdit::group() const
{
    if (isEnabled()) {
        return d->mGroup;
    } else {
        return KeyGroup();
    }
}

QString CertificateLineEdit::Private::text() const
{
    return ui.lineEdit.text();
}

QString CertificateLineEdit::text() const
{
    return d->text();
}

void CertificateLineEdit::Private::setKey(const Key &key)
{
    mKey = key;
    mGroup = KeyGroup();
    qCDebug(KLEOPATRA_LOG) << "Setting Key. " << Formatting::summaryLine(key);
    setTextWithBlockedSignals(Formatting::summaryLine(key));
    updateKey();
}

void CertificateLineEdit::setKey(const Key &key)
{
    d->setKey(key);
}

void CertificateLineEdit::Private::setGroup(const KeyGroup &group)
{
    mGroup = group;
    mKey = Key();
    const QString summary = Formatting::summaryLine(group);
    qCDebug(KLEOPATRA_LOG) << "Setting KeyGroup. " << summary;
    setTextWithBlockedSignals(summary);
    updateKey();
}

void CertificateLineEdit::setGroup(const KeyGroup &group)
{
    d->setGroup(group);
}

bool CertificateLineEdit::isEmpty() const
{
    return d->mStatus == Private::Status::Empty;
}

void CertificateLineEdit::Private::setKeyFilter(const std::shared_ptr<KeyFilter> &filter)
{
    mFilter = filter;
    mFilterModel->setKeyFilter(filter);
    mCompleterFilterModel->setKeyFilter(mFilter);
    updateKey();
}

void CertificateLineEdit::setKeyFilter(const std::shared_ptr<KeyFilter> &filter)
{
    d->setKeyFilter(filter);
}

void CertificateLineEdit::setAccessibleNameOfLineEdit(const QString &name)
{
    d->ui.lineEdit.setAccessibleName(name);
}

#include "certificatelineedit.moc"
