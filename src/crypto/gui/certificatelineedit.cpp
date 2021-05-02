/*  crypto/gui/certificatelineedit.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
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
#include <Libkleo/KeyList>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeyListSortFilterProxyModel>
#include <Libkleo/Formatting>

#include <KLocalizedString>

#include <gpgme++/key.h>

#include <QGpgME/KeyForMailboxJob>
#include <QGpgME/Protocol>

using namespace Kleo;
using namespace GpgME;

Q_DECLARE_METATYPE(GpgME::Key)
Q_DECLARE_METATYPE(KeyGroup)

static QStringList s_lookedUpKeys;

namespace
{
class ProxyModel : public KeyListSortFilterProxyModel
{
    Q_OBJECT

public:
    ProxyModel(QObject *parent = nullptr)
        : KeyListSortFilterProxyModel(parent)
    {
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid()) {
            return QVariant();
        }

        switch (role) {
        case Qt::DecorationRole: {
            const auto key = KeyListSortFilterProxyModel::data(index, KeyList::KeyRole).value<GpgME::Key>();
            if (!key.isNull()) {
                return Kleo::Formatting::iconForUid(key.userID(0));
            }

            const auto group = KeyListSortFilterProxyModel::data(index, KeyList::GroupRole).value<KeyGroup>();
            if (!group.isNull()) {
                return QIcon::fromTheme(QStringLiteral("group"));
            }

            Q_ASSERT(!key.isNull() || !group.isNull());
            return QVariant();
        }
        default:
            return KeyListSortFilterProxyModel::data(index, role);
        }
    }
};
} // namespace

CertificateLineEdit::CertificateLineEdit(AbstractKeyListModel *model,
                                         QWidget *parent,
                                         KeyFilter *filter)
    : QLineEdit(parent),
      mFilterModel(new KeyListSortFilterProxyModel(this)),
      mCompleterFilterModel(new ProxyModel(this)),
      mCompleter(new QCompleter(this)),
      mFilter(std::shared_ptr<KeyFilter>(filter)),
      mLineAction(new QAction(this))
{
    setPlaceholderText(i18n("Please enter a name or email address..."));
    setClearButtonEnabled(true);
    addAction(mLineAction, QLineEdit::LeadingPosition);

    mCompleterFilterModel->setKeyFilter(mFilter);
    mCompleterFilterModel->setSourceModel(model);
    mCompleter->setModel(mCompleterFilterModel);
    mCompleter->setCompletionColumn(KeyList::Summary);
    mCompleter->setFilterMode(Qt::MatchContains);
    mCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    setCompleter(mCompleter);
    mFilterModel->setSourceModel(model);
    mFilterModel->setFilterKeyColumn(KeyList::Summary);
    if (filter) {
        mFilterModel->setKeyFilter(mFilter);
    }

    connect(KeyCache::instance().get(), &Kleo::KeyCache::keyListingDone,
            this, &CertificateLineEdit::updateKey);
    connect(KeyCache::instance().get(), &Kleo::KeyCache::groupUpdated,
            this, [this] (const KeyGroup &group) {
                if (!mGroup.isNull() && mGroup.source() == group.source() && mGroup.id() == group.id()) {
                    QSignalBlocker blocky(this);
                    setText(Formatting::summaryLine(group));
                    // queue the update to ensure that the model has been updated
                    QMetaObject::invokeMethod(this, &CertificateLineEdit::updateKey, Qt::QueuedConnection);
                }
            });
    connect(KeyCache::instance().get(), &Kleo::KeyCache::groupRemoved,
            this, [this] (const KeyGroup &group) {
                if (!mGroup.isNull() && mGroup.source() == group.source() && mGroup.id() == group.id()) {
                    mGroup = KeyGroup();
                    QSignalBlocker blocky(this);
                    clear();
                    // queue the update to ensure that the model has been updated
                    QMetaObject::invokeMethod(this, &CertificateLineEdit::updateKey, Qt::QueuedConnection);
                }
            });
    connect(this, &QLineEdit::editingFinished,
            this, [this] () {
                // queue the call of editFinished() to ensure that QCompleter::activated is handled first
                QMetaObject::invokeMethod(this, &CertificateLineEdit::editFinished, Qt::QueuedConnection);
            });
    connect(this, &QLineEdit::textChanged,
            this, &CertificateLineEdit::editChanged);
    connect(mLineAction, &QAction::triggered,
            this, &CertificateLineEdit::dialogRequested);
    connect(mCompleter, QOverload<const QModelIndex &>::of(&QCompleter::activated),
            this, [this] (const QModelIndex &index) {
                Key key = mCompleter->completionModel()->data(index, KeyList::KeyRole).value<Key>();
                auto group = mCompleter->completionModel()->data(index, KeyList::GroupRole).value<KeyGroup>();
                if (!key.isNull()) {
                    setKey(key);
                } else if (!group.isNull()) {
                    setGroup(group);
                } else {
                    qCDebug(KLEOPATRA_LOG) << "Activated item is neither key nor group";
                }
            });
    updateKey();

    /* Take ownership of the model to prevent double deletion when the
     * filter models are deleted */
    model->setParent(parent ? parent : this);
}

void CertificateLineEdit::editChanged()
{
    updateKey();
    if (!mEditStarted) {
        Q_EMIT editingStarted();
        mEditStarted = true;
    }
    mEditFinished = false;
}

void CertificateLineEdit::editFinished()
{
    mEditStarted = false;
    mEditFinished = true;
    updateKey();
    checkLocate();
}

void CertificateLineEdit::checkLocate()
{
    if (!key().isNull() || !group().isNull()) {
        // Already have a key or group
        return;
    }

    // Only check once per mailbox
    const auto mailText = text();
    if (s_lookedUpKeys.contains(mailText)) {
        return;
    }
    s_lookedUpKeys << mailText;
    qCDebug(KLEOPATRA_LOG) << "Lookup job for" << mailText;
    QGpgME::KeyForMailboxJob *job = QGpgME::openpgp()->keyForMailboxJob();
    job->start(mailText);
}

void CertificateLineEdit::updateKey()
{
    static const _detail::ByFingerprint<std::equal_to> keysHaveSameFingerprint;

    const auto mailText = text();
    auto newKey = Key();
    auto newGroup = KeyGroup();
    if (mailText.isEmpty()) {
        mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("resource-group-new")));
        mLineAction->setToolTip(i18n("Open selection dialog."));
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
                    mLineAction->setToolTip(i18n("Multiple certificates"));
                } else {
                    mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("resource-group-new")));
                    mLineAction->setToolTip(i18n("Open selection dialog."));
                }
            }
        } else if (mFilterModel->rowCount() == 1) {
            const auto index = mFilterModel->index(0, 0);
            newKey = mFilterModel->data(index, KeyList::KeyRole).value<Key>();
            newGroup = mFilterModel->data(index, KeyList::GroupRole).value<KeyGroup>();
            Q_ASSERT(!newKey.isNull() || !newGroup.isNull());
            if (newKey.isNull() && newGroup.isNull()) {
                mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("emblem-error")));
                mLineAction->setToolTip(i18n("No matching certificates found.<br/>Click here to import a certificate."));
            }
        } else {
            mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("emblem-error")));
            mLineAction->setToolTip(i18n("No matching certificates found.<br/>Click here to import a certificate."));
        }
    }
    mKey = newKey;
    mGroup = newGroup;

    if (!mKey.isNull()) {
        /* FIXME: This needs to be solved by a multiple UID supporting model */
        mLineAction->setIcon(Formatting::iconForUid(mKey.userID(0)));
        mLineAction->setToolTip(Formatting::validity(mKey.userID(0)) +
                                QLatin1String("<br/>") + i18n("Click for details."));
        setToolTip(Formatting::toolTip(mKey, Formatting::ToolTipOption::AllOptions));
    } else if (!mGroup.isNull()) {
        mLineAction->setIcon(Formatting::validityIcon(mGroup));
        mLineAction->setToolTip(Formatting::validity(mGroup) +
                                QLatin1String("<br/>") + i18n("Click for details."));
        setToolTip(Formatting::toolTip(mGroup, Formatting::ToolTipOption::AllOptions));
    } else {
        setToolTip(QString());
    }

    Q_EMIT keyChanged();

    if (mailText.isEmpty()) {
        Q_EMIT wantsRemoval(this);
    }
}

Key CertificateLineEdit::key() const
{
    if (isEnabled()) {
        return mKey;
    } else {
        return Key();
    }
}

KeyGroup CertificateLineEdit::group() const
{
    if (isEnabled()) {
        return mGroup;
    } else {
        return KeyGroup();
    }
}

void CertificateLineEdit::setKey(const Key &key)
{
    mKey = key;
    mGroup = KeyGroup();
    QSignalBlocker blocky(this);
    qCDebug(KLEOPATRA_LOG) << "Setting Key. " << Formatting::summaryLine(key);
    setText(Formatting::summaryLine(key));
    updateKey();
}

void CertificateLineEdit::setGroup(const KeyGroup &group)
{
    mGroup = group;
    mKey = Key();
    QSignalBlocker blocky(this);
    const QString summary = Formatting::summaryLine(group);
    qCDebug(KLEOPATRA_LOG) << "Setting KeyGroup. " << summary;
    setText(summary);
    updateKey();
}

bool CertificateLineEdit::isEmpty() const
{
    return text().isEmpty();
}

void CertificateLineEdit::setKeyFilter(const std::shared_ptr<KeyFilter> &filter)
{
    mFilter = filter;
    mFilterModel->setKeyFilter(filter);
    mCompleterFilterModel->setKeyFilter(mFilter);
    updateKey();
}

#include "certificatelineedit.moc"
