/*
    dialogs/subkeyswidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "subkeyswidget.h"

#include "commands/changeexpirycommand.h"
#include "commands/exportsecretsubkeycommand.h"
#include "commands/importpaperkeycommand.h"
#include "commands/keytocardcommand.h"
#include "exportdialog.h"

#include <kleopatra_debug.h>

#include <Libkleo/Formatting>
#include <Libkleo/KeyCache>
#include <Libkleo/KeyHelpers>
#include <Libkleo/TreeWidget>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <gpgme++/context.h>
#include <gpgme++/key.h>

Q_DECLARE_METATYPE(GpgME::Subkey)

using namespace Kleo;
using namespace Kleo::Commands;

class SubKeysWidget::Private
{
    SubKeysWidget *const q;

public:
    Private(SubKeysWidget *qq)
        : q{qq}
        , ui{qq}
    {
        ui.subkeysTree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui.subkeysTree, &QAbstractItemView::customContextMenuRequested, q, [this](const QPoint &p) {
            tableContextMenuRequested(p);
        });
        connect(Kleo::KeyCache::instance().get(), &Kleo::KeyCache::keysMayHaveChanged, q, [this]() {
            keysMayHaveChanged();
        });
    }

    void changeValidity(const GpgME::Subkey &subkey);
    void exportSSH(const GpgME::Subkey &subkey);
    void keyToCard(const GpgME::Subkey &subkey);
    void exportSecret(const GpgME::Subkey &subkey);
    void importPaperKey();

private:
    void tableContextMenuRequested(const QPoint &p);
    void keysMayHaveChanged();

public:
    GpgME::Key key;

public:
    struct UI {
        QVBoxLayout *mainLayout;
        TreeWidget *subkeysTree;

        QPushButton *changeValidityBtn = nullptr;
        QPushButton *exportOpenSSHBtn = nullptr;
        QPushButton *restoreBtn = nullptr;
        QPushButton *transferToSmartcardBtn = nullptr;
        QPushButton *exportSecretBtn = nullptr;

        UI(QWidget *widget)
        {
            mainLayout = new QVBoxLayout{widget};
            mainLayout->setContentsMargins(0, 0, 0, 0);

            auto subkeysTreeLabel = new QLabel{i18nc("@label", "Subkeys:"), widget};
            mainLayout->addWidget(subkeysTreeLabel);

            subkeysTree = new TreeWidget{widget};
            subkeysTreeLabel->setBuddy(subkeysTree);
            subkeysTree->setAccessibleName(i18nc("@label", "Subkeys"));
            subkeysTree->setRootIsDecorated(false);
            subkeysTree->setHeaderLabels({
                i18nc("@title:column", "ID"),
                i18nc("@title:column", "Type"),
                i18nc("@title:column", "Valid From"),
                i18nc("@title:column", "Valid Until"),
                i18nc("@title:column", "Status"),
                i18nc("@title:column", "Strength"),
                i18nc("@title:column", "Usage"),
                i18nc("@title:column", "Primary"),
                i18nc("@title:column", "Storage"),
            });
            mainLayout->addWidget(subkeysTree);

            {
                auto buttonRow = new QHBoxLayout;

                changeValidityBtn = new QPushButton(i18nc("@action:button", "Change validity"), widget);
                buttonRow->addWidget(changeValidityBtn);

                exportOpenSSHBtn = new QPushButton{i18nc("@action:button", "Export OpenSSH key"), widget};
                buttonRow->addWidget(exportOpenSSHBtn);

                restoreBtn = new QPushButton(i18nc("@action:button", "Restore printed backup"), widget);
                buttonRow->addWidget(restoreBtn);

                transferToSmartcardBtn = new QPushButton(i18nc("@action:button", "Transfer to smartcard"), widget);
                buttonRow->addWidget(transferToSmartcardBtn);

                exportSecretBtn = new QPushButton(i18nc("@action:button", "Export secret subkey"), widget);
                buttonRow->addWidget(exportSecretBtn);

                buttonRow->addStretch(1);

                mainLayout->addLayout(buttonRow);
            }
        }
    } ui;
};

void SubKeysWidget::Private::changeValidity(const GpgME::Subkey &subkey)
{
    auto cmd = new ChangeExpiryCommand(subkey.parent());
    cmd->setSubkey(subkey);
    ui.subkeysTree->setEnabled(false);
    connect(cmd, &ChangeExpiryCommand::finished, q, [this]() {
        ui.subkeysTree->setEnabled(true);
        key.update();
        q->setKey(key);
    });
    cmd->setParentWidget(q);
    cmd->start();
}

void SubKeysWidget::Private::exportSSH(const GpgME::Subkey &subkey)
{
    QScopedPointer<ExportDialog> dlg(new ExportDialog(q));
    dlg->setKey(subkey, static_cast<unsigned int>(GpgME::Context::ExportSSH));
    dlg->exec();
}

void SubKeysWidget::Private::importPaperKey()
{
    auto cmd = new ImportPaperKeyCommand(key);
    ui.subkeysTree->setEnabled(false);
    connect(cmd, &ImportPaperKeyCommand::finished, q, [this]() {
        ui.subkeysTree->setEnabled(true);
    });
    cmd->setParentWidget(q);
    cmd->start();
}

void SubKeysWidget::Private::keyToCard(const GpgME::Subkey &subkey)
{
    auto cmd = new KeyToCardCommand(subkey);
    ui.subkeysTree->setEnabled(false);
    connect(cmd, &KeyToCardCommand::finished, q, [this]() {
        ui.subkeysTree->setEnabled(true);
    });
    cmd->setParentWidget(q);
    cmd->start();
}

void SubKeysWidget::Private::exportSecret(const GpgME::Subkey &subkey)
{
    auto cmd = new ExportSecretSubkeyCommand{{subkey}};
    ui.subkeysTree->setEnabled(false);
    connect(cmd, &ExportSecretSubkeyCommand::finished, q, [this]() {
        ui.subkeysTree->setEnabled(true);
    });
    cmd->setParentWidget(q);
    cmd->start();
}

void SubKeysWidget::Private::tableContextMenuRequested(const QPoint &p)
{
    auto item = ui.subkeysTree->itemAt(p);
    if (!item) {
        return;
    }
    const auto subkey = item->data(0, Qt::UserRole).value<GpgME::Subkey>();
    const bool isOwnKey = subkey.parent().hasSecret();
    const bool secretSubkeyStoredInKeyRing = subkey.isSecret() && !subkey.isCardKey();

    auto menu = new QMenu(q);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

    if (isOwnKey) {
        auto action = menu->addAction(QIcon::fromTheme(QStringLiteral("change-date-symbolic")), i18n("Change validity"), q, [this, subkey]() {
            changeValidity(subkey);
        });
        action->setEnabled(canBeUsedForSecretKeyOperations(subkey.parent()));
    }

    if (subkey.canAuthenticate()) {
        menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-export")), i18n("Export OpenSSH key"), q, [this, subkey]() {
            exportSSH(subkey);
        });
    }

    auto action = menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-import")), i18n("Restore printed backup"), q, [this, subkey]() {
        importPaperKey();
    });

    action->setEnabled(!secretSubkeyStoredInKeyRing);

    if (isOwnKey) {
        auto action = menu->addAction(QIcon::fromTheme(QStringLiteral("send-to-symbolic")), i18n("Transfer to smartcard"), q, [this, subkey]() {
            keyToCard(subkey);
        });
        action->setEnabled(secretSubkeyStoredInKeyRing && !KeyToCardCommand::getSuitableCards(subkey).empty());
    }

    const bool isPrimarySubkey = subkey.keyID() == key.keyID();
    if (isOwnKey && !isPrimarySubkey) {
        auto action = menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-export")), i18n("Export secret subkey"), q, [this, subkey]() {
            exportSecret(subkey);
        });
        action->setEnabled(secretSubkeyStoredInKeyRing);
    }

    menu->popup(ui.subkeysTree->viewport()->mapToGlobal(p));
}

void SubKeysWidget::Private::keysMayHaveChanged()
{
    qCDebug(KLEOPATRA_LOG) << q << __func__;
    const auto updatedKey = Kleo::KeyCache::instance()->findByFingerprint(key.primaryFingerprint());
    if (!updatedKey.isNull()) {
        q->setKey(updatedKey);
    }
}

SubKeysWidget::SubKeysWidget(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{
    connect(d->ui.subkeysTree, &TreeWidget::currentItemChanged, this, [this] {
        const auto currentIndex = d->ui.subkeysTree->currentIndex().row();
        const auto &subkey = d->key.subkey(currentIndex);
        const bool secretSubkeyStoredInKeyRing = subkey.isSecret() && !subkey.isCardKey();
        d->ui.exportOpenSSHBtn->setEnabled(subkey.canAuthenticate());
        d->ui.changeValidityBtn->setEnabled(d->key.hasSecret() && canBeUsedForSecretKeyOperations(subkey.parent()));
        d->ui.exportSecretBtn->setEnabled(d->key.hasSecret() && subkey.fingerprint() != d->key.primaryFingerprint() && secretSubkeyStoredInKeyRing);
        d->ui.restoreBtn->setEnabled(!secretSubkeyStoredInKeyRing);
        d->ui.transferToSmartcardBtn->setEnabled(secretSubkeyStoredInKeyRing && !KeyToCardCommand::getSuitableCards(subkey).empty());
    });
    connect(d->ui.changeValidityBtn, &QPushButton::clicked, this, [this] {
        d->changeValidity(d->key.subkey(d->ui.subkeysTree->currentIndex().row()));
    });
    connect(d->ui.exportOpenSSHBtn, &QPushButton::clicked, this, [this] {
        d->exportSSH(d->key.subkey(d->ui.subkeysTree->currentIndex().row()));
    });
    connect(d->ui.restoreBtn, &QPushButton::clicked, this, [this] {
        d->importPaperKey();
    });
    connect(d->ui.transferToSmartcardBtn, &QPushButton::clicked, this, [this] {
        d->keyToCard(d->key.subkey(d->ui.subkeysTree->currentIndex().row()));
    });
    connect(d->ui.exportSecretBtn, &QPushButton::clicked, this, [this] {
        d->exportSecret(d->key.subkey(d->ui.subkeysTree->currentIndex().row()));
    });
}

SubKeysWidget::~SubKeysWidget() = default;

void SubKeysWidget::setKey(const GpgME::Key &key)
{
    if (key.protocol() != GpgME::OpenPGP) {
        return;
    }
    d->key = key;

    const auto currentItem = d->ui.subkeysTree->currentItem();
    const QByteArray selectedKeyFingerprint = currentItem ? QByteArray(currentItem->data(0, Qt::UserRole).value<GpgME::Subkey>().fingerprint()) : QByteArray();
    d->ui.subkeysTree->clear();

    const auto subkeys = key.subkeys();
    for (const auto &subkey : subkeys) {
        auto item = new QTreeWidgetItem;
        item->setData(0, Qt::DisplayRole, Formatting::prettyID(subkey.keyID()));
        item->setData(0, Qt::AccessibleTextRole, Formatting::accessibleHexID(subkey.keyID()));
        item->setData(0, Qt::UserRole, QVariant::fromValue(subkey));
        item->setData(1, Qt::DisplayRole, Kleo::Formatting::type(subkey));
        item->setData(2, Qt::DisplayRole, Kleo::Formatting::creationDateString(subkey));
        item->setData(2, Qt::AccessibleTextRole, Formatting::accessibleCreationDate(subkey));
        item->setData(3,
                      Qt::DisplayRole,
                      subkey.neverExpires() ? Kleo::Formatting::expirationDateString(subkey.parent()) : Kleo::Formatting::expirationDateString(subkey));
        item->setData(3,
                      Qt::AccessibleTextRole,
                      subkey.neverExpires() ? Kleo::Formatting::accessibleExpirationDate(subkey.parent()) : Kleo::Formatting::accessibleExpirationDate(subkey));
        item->setData(4, Qt::DisplayRole, Kleo::Formatting::validityShort(subkey));
        switch (subkey.publicKeyAlgorithm()) {
        case GpgME::Subkey::AlgoECDSA:
        case GpgME::Subkey::AlgoEDDSA:
        case GpgME::Subkey::AlgoECDH:
            item->setData(5, Qt::DisplayRole, QString::fromStdString(subkey.algoName()));
            break;
        default:
            item->setData(5, Qt::DisplayRole, QString::number(subkey.length()));
        }
        item->setData(6, Qt::DisplayRole, Kleo::Formatting::usageString(subkey));
        const auto isPrimary = subkey.keyID() == key.keyID();
        item->setData(7, Qt::DisplayRole, isPrimary ? QStringLiteral("✓") : QString());
        item->setData(7, Qt::AccessibleTextRole, isPrimary ? i18nc("yes, is primary key", "yes") : i18nc("no, is not primary key", "no"));
        if (subkey.isCardKey()) {
            if (const char *serialNo = subkey.cardSerialNumber()) {
                item->setData(8, Qt::DisplayRole, i18nc("smart card <serial number>", "smart card %1", QString::fromUtf8(serialNo)));
            } else {
                item->setData(8, Qt::DisplayRole, i18n("smart card"));
            }
        } else if (isPrimary && key.hasSecret() && !subkey.isSecret()) {
            item->setData(8, Qt::DisplayRole, i18nc("key is 'offline key', i.e. secret key is not stored on this computer", "offline"));
        } else if (subkey.isSecret()) {
            item->setData(8, Qt::DisplayRole, i18n("on this computer"));
        } else {
            item->setData(8, Qt::DisplayRole, i18nc("unknown storage location", "unknown"));
        }
        d->ui.subkeysTree->addTopLevelItem(item);
        if (subkey.fingerprint() == selectedKeyFingerprint) {
            d->ui.subkeysTree->setCurrentItem(item);
        }
    }
    if (!key.hasSecret()) {
        // hide information about storage location for keys of other people
        d->ui.subkeysTree->hideColumn(8);
    }
    d->ui.subkeysTree->header()->resizeSections(QHeaderView::ResizeToContents);

    d->ui.changeValidityBtn->setVisible(key.hasSecret());
    d->ui.exportSecretBtn->setVisible(key.hasSecret());
    d->ui.transferToSmartcardBtn->setVisible(key.hasSecret());
}

GpgME::Key SubKeysWidget::key() const
{
    return d->key;
}

SubKeysDialog::SubKeysDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18nc("@title:window", "Subkeys Details"));
    auto l = new QVBoxLayout(this);
    l->addWidget(new SubKeysWidget(this));

    auto bbox = new QDialogButtonBox(this);
    auto btn = bbox->addButton(QDialogButtonBox::Close);
    connect(btn, &QPushButton::clicked, this, &QDialog::accept);
    l->addWidget(bbox);
    readConfig();
}

SubKeysDialog::~SubKeysDialog()
{
    writeConfig();
}

void SubKeysDialog::readConfig()
{
    KConfigGroup dialog(KSharedConfig::openStateConfig(), QStringLiteral("SubKeysDialog"));
    const QSize size = dialog.readEntry("Size", QSize(820, 280));
    if (size.isValid()) {
        resize(size);
    }
}

void SubKeysDialog::writeConfig()
{
    KConfigGroup dialog(KSharedConfig::openStateConfig(), QStringLiteral("SubKeysDialog"));
    dialog.writeEntry("Size", size());
    dialog.sync();
}

void SubKeysDialog::setKey(const GpgME::Key &key)
{
    auto w = findChild<SubKeysWidget *>();
    Q_ASSERT(w);
    w->setKey(key);
}

GpgME::Key SubKeysDialog::key() const
{
    auto w = findChild<SubKeysWidget *>();
    Q_ASSERT(w);
    return w->key();
}

#include "moc_subkeyswidget.cpp"
