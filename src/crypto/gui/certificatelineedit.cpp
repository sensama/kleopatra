/*  crypto/gui/certificatelineedit.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2021, 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "certificatelineedit.h"

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

#include <QGpgME/KeyForMailboxJob>
#include <QGpgME/Protocol>

#include <QLabel>

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
} // namespace

class CertificateLineEdit::Private
{
    CertificateLineEdit *q;

public:
    explicit Private(CertificateLineEdit *qq, AbstractKeyListModel *model, KeyFilter *filter);

    void setKeyFilter(const std::shared_ptr<KeyFilter> &filter);

    void updateKey();
    void editChanged();
    void editFinished();
    void checkLocate();

public:
    GpgME::Key mKey;
    KeyGroup mGroup;

private:
    KeyListSortFilterProxyModel *const mFilterModel;
    KeyListSortFilterProxyModel *const mCompleterFilterModel;
    QCompleter *mCompleter = nullptr;
    std::shared_ptr<KeyFilter> mFilter;
    bool mEditStarted = false;
    bool mEditFinished = false;
    QAction *const mLineAction;
};

CertificateLineEdit::Private::Private(CertificateLineEdit *qq, AbstractKeyListModel *model, KeyFilter *filter)
    : q{qq}
    , mFilterModel{new KeyListSortFilterProxyModel{qq}}
    , mCompleterFilterModel{new CompletionProxyModel{qq}}
    , mCompleter{new QCompleter{qq}}
    , mFilter{std::shared_ptr<KeyFilter>{filter}}
    , mLineAction{new QAction{qq}}
{
    q->setPlaceholderText(i18n("Please enter a name or email address..."));
    q->setClearButtonEnabled(true);
    q->addAction(mLineAction, QLineEdit::LeadingPosition);

    mCompleterFilterModel->setKeyFilter(mFilter);
    mCompleterFilterModel->setSourceModel(model);
    mCompleter->setModel(mCompleterFilterModel);
    mCompleter->setFilterMode(Qt::MatchContains);
    mCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    q->setCompleter(mCompleter);
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
                    QSignalBlocker blocky{q};
                    q->setText(Formatting::summaryLine(group));
                    // queue the update to ensure that the model has been updated
                    QMetaObject::invokeMethod(q, [this]() { updateKey(); }, Qt::QueuedConnection);
                }
            });
    connect(KeyCache::instance().get(), &Kleo::KeyCache::groupRemoved,
            q, [this](const KeyGroup &group) {
                if (!mGroup.isNull() && mGroup.source() == group.source() && mGroup.id() == group.id()) {
                    mGroup = KeyGroup();
                    QSignalBlocker blocky{q};
                    q->clear();
                    // queue the update to ensure that the model has been updated
                    QMetaObject::invokeMethod(q, [this]() { updateKey(); }, Qt::QueuedConnection);
                }
            });
    connect(q, &QLineEdit::editingFinished,
            q, [this]() {
                // queue the call of editFinished() to ensure that QCompleter::activated is handled first
                QMetaObject::invokeMethod(q, [this]() { editFinished(); }, Qt::QueuedConnection);
            });
    connect(q, &QLineEdit::textChanged,
            q, [this]() { editChanged(); });
    connect(mLineAction, &QAction::triggered,
            q, &CertificateLineEdit::dialogRequested);
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

CertificateLineEdit::CertificateLineEdit(AbstractKeyListModel *model,
                                         QWidget *parent,
                                         KeyFilter *filter)
    : QLineEdit(parent)
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
        QSignalBlocker blocky{q};
        q->setText(Formatting::summaryLine(q->key()));
    } else if (!q->group().isNull()) {
        QSignalBlocker blocky{q};
        q->setText(Formatting::summaryLine(q->group()));
    } else {
        checkLocate();
    }
}

void CertificateLineEdit::Private::checkLocate()
{
    if (!q->key().isNull() || !q->group().isNull()) {
        // Already have a key or group
        return;
    }

    // Only check once per mailbox
    const auto mailText = q->text();
    if (s_lookedUpKeys.contains(mailText)) {
        return;
    }
    s_lookedUpKeys << mailText;
    qCDebug(KLEOPATRA_LOG) << "Lookup job for" << mailText;
    QGpgME::KeyForMailboxJob *job = QGpgME::openpgp()->keyForMailboxJob();
    job->start(mailText);
}

void CertificateLineEdit::Private::updateKey()
{
    static const _detail::ByFingerprint<std::equal_to> keysHaveSameFingerprint;

    const auto mailText = q->text();
    auto newKey = Key();
    auto newGroup = KeyGroup();
    if (mailText.isEmpty()) {
        mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("resource-group-new")));
        mLineAction->setToolTip(i18n("Open selection dialog."));
        q->setToolTip({});
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
                if (mEditFinished) {
                    mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("question")));
                    mLineAction->setToolTip(i18n("Multiple matching certificates found"));
                    q->setToolTip(i18n("Multiple matching certificates found"));
                } else {
                    mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("resource-group-new")));
                    mLineAction->setToolTip(i18n("Open selection dialog."));
                    q->setToolTip({});
                }
            }
        } else if (mFilterModel->rowCount() == 1) {
            const auto index = mFilterModel->index(0, 0);
            newKey = mFilterModel->data(index, KeyList::KeyRole).value<Key>();
            newGroup = mFilterModel->data(index, KeyList::GroupRole).value<KeyGroup>();
            Q_ASSERT(!newKey.isNull() || !newGroup.isNull());
            if (newKey.isNull() && newGroup.isNull()) {
                mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("emblem-error")));
                mLineAction->setToolTip(i18n("No matching certificates found.<br/>Click to import a certificate."));
                q->setToolTip(i18n("No matching certificates found"));
            }
        } else {
            mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("emblem-error")));
            mLineAction->setToolTip(i18n("No matching certificates found.<br/>Click to import a certificate."));
            q->setToolTip(i18n("No matching certificates found"));
        }
    }
    mKey = newKey;
    mGroup = newGroup;

    if (!mKey.isNull()) {
        /* FIXME: This needs to be solved by a multiple UID supporting model */
        mLineAction->setIcon(Formatting::iconForUid(mKey.userID(0)));
        mLineAction->setToolTip(Formatting::validity(mKey.userID(0)) +
                                QLatin1String("<br/>") + i18n("Click for details."));
        q->setToolTip(Formatting::toolTip(mKey, Formatting::ToolTipOption::AllOptions));
    } else if (!mGroup.isNull()) {
        mLineAction->setIcon(Formatting::validityIcon(mGroup));
        mLineAction->setToolTip(Formatting::validity(mGroup) +
                                QLatin1String("<br/>") + i18n("Click for details."));
        q->setToolTip(Formatting::toolTip(mGroup, Formatting::ToolTipOption::AllOptions));
    }

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

void CertificateLineEdit::setKey(const Key &key)
{
    d->mKey = key;
    d->mGroup = KeyGroup();
    QSignalBlocker blocky{this};
    qCDebug(KLEOPATRA_LOG) << "Setting Key. " << Formatting::summaryLine(key);
    setText(Formatting::summaryLine(key));
    d->updateKey();
}

void CertificateLineEdit::setGroup(const KeyGroup &group)
{
    d->mGroup = group;
    d->mKey = Key();
    QSignalBlocker blocky{this};
    const QString summary = Formatting::summaryLine(group);
    qCDebug(KLEOPATRA_LOG) << "Setting KeyGroup. " << summary;
    setText(summary);
    d->updateKey();
}

bool CertificateLineEdit::isEmpty() const
{
    return text().isEmpty();
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

#include "certificatelineedit.moc"
