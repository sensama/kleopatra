/*  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "subkeyswidget.h"
#include "ui_subkeyswidget.h"

#include "smartcard/openpgpcard.h"
#include "smartcard/readerstatus.h"

#include "commands/changeexpirycommand.h"
#include "commands/keytocardcommand.h"
#include "commands/importpaperkeycommand.h"
#include "exportdialog.h"

#include <gpgme++/key.h>
#include <gpgme++/context.h>

#include <KConfigGroup>
#include <KSharedConfig>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTreeWidgetItem>
#include <QMenu>

#include <gpgme++/gpgmepp_version.h>
#if GPGMEPP_VERSION >= 0x10E00 // 1.14.0
# define GPGME_HAS_EXPORT_FLAGS
#endif
#if GPGMEPP_VERSION >= 0x10E01 // 1.14.1
# define CHANGEEXPIRYJOB_SUPPORTS_SUBKEYS
#endif

#include <Libkleo/Formatting>

Q_DECLARE_METATYPE(GpgME::Subkey)

using namespace Kleo;

class SubKeysWidget::Private
{
public:
    Private(SubKeysWidget *q)
        : q(q)
    {
        ui.setupUi(q);
        ui.subkeysTree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui.subkeysTree, &QAbstractItemView::customContextMenuRequested,
                q, [this](const QPoint &p) { tableContextMenuRequested(p); });
    }

    GpgME::Key key;
    Ui::SubKeysWidget ui;
    void tableContextMenuRequested(const QPoint &p);
private:
    SubKeysWidget *const q;
};

void SubKeysWidget::Private::tableContextMenuRequested(const QPoint &p)
{
    auto item = ui.subkeysTree->itemAt(p);
    if (!item) {
        return;
    }
    const auto subkey = item->data(0, Qt::UserRole).value<GpgME::Subkey>();

    QMenu *menu = new QMenu(q);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

    bool hasActions = false;

#ifdef CHANGEEXPIRYJOB_SUPPORTS_SUBKEYS
    if (subkey.parent().protocol() == GpgME::OpenPGP && subkey.parent().hasSecret()) {
        hasActions = true;
        menu->addAction(i18n("Change Expiry Date..."), q,
                [this, subkey]() {
                    auto cmd = new Kleo::Commands::ChangeExpiryCommand(subkey.parent());
                    if (subkey.keyID() != key.keyID()) {
                        // do not set the primary key as subkey
                        cmd->setSubkey(subkey);
                    }
                    ui.subkeysTree->setEnabled(false);
                    connect(cmd, &Kleo::Commands::ChangeExpiryCommand::finished,
                            q, [this]() {
                                ui.subkeysTree->setEnabled(true);
                                key.update();
                                q->setKey(key);
                            });
                    cmd->setParentWidget(q);
                    cmd->start();
                }
        );
    }
#endif // CHANGEEXPIRYJOB_SUPPORTS_SUBKEYS

#ifdef GPGME_HAS_EXPORT_FLAGS
    if (subkey.parent().protocol() == GpgME::OpenPGP && subkey.canAuthenticate()) {
        hasActions = true;
        menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-export")),
                i18n("Export OpenSSH key"),
                q, [this, subkey]() {
            QScopedPointer<ExportDialog> dlg(new ExportDialog(q));
            dlg->setKey(subkey, static_cast<unsigned int> (GpgME::Context::ExportSSH));
            dlg->exec();
        });
    }
#endif // GPGME_HAS_EXPORT_FLAGS

    if (!subkey.isSecret()) {
        hasActions = true;
        menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-import")),
                        i18n("Restore printed backup"),
                        q, [this, subkey] () {
            auto cmd = new Kleo::Commands::ImportPaperKeyCommand(subkey.parent());
            ui.subkeysTree->setEnabled(false);
            connect(cmd, &Kleo::Commands::ImportPaperKeyCommand::finished,
                    q, [this]() { ui.subkeysTree->setEnabled(true); });
            cmd->setParentWidget(q);
            cmd->start();
        });
    }

    if (subkey.isSecret() && Kleo::Commands::KeyToCardCommand::supported()) {
        const auto cards = SmartCard::ReaderStatus::instance()->getCards();
        if (cards.size() && cards[0]->appName() == SmartCard::OpenPGPCard::AppName) {
            const auto card = cards[0];

            if (!subkey.cardSerialNumber() || card->serialNumber() != subkey.cardSerialNumber()) {
                hasActions = true;
                menu->addAction(QIcon::fromTheme(QStringLiteral("send-to-symbolic")),
                                i18n("Transfer to smartcard"),
                                q, [this, subkey, card]() {
                    auto cmd = new Kleo::Commands::KeyToCardCommand(subkey, card->serialNumber(), card->appName());
                    ui.subkeysTree->setEnabled(false);
                    connect(cmd, &Kleo::Commands::KeyToCardCommand::finished,
                            q, [this]() { ui.subkeysTree->setEnabled(true); });
                    cmd->setParentWidget(q);
                    cmd->start();
                });
            }
        }
    }

    if (hasActions) {
        menu->popup(ui.subkeysTree->viewport()->mapToGlobal(p));
    } else {
        delete menu;
    }
}

SubKeysWidget::SubKeysWidget(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{
}

SubKeysWidget::~SubKeysWidget()
{
}

void SubKeysWidget::setKey(const GpgME::Key &key)
{
    d->key = key;

    const auto currentItem = d->ui.subkeysTree->currentItem();
    const QByteArray selectedKeyFingerprint = currentItem ?
        QByteArray(currentItem->data(0, Qt::UserRole).value<GpgME::Subkey>().fingerprint()) : QByteArray();
    d->ui.subkeysTree->clear();

    for (const auto &subkey : key.subkeys()) {
        auto item = new QTreeWidgetItem();
        item->setData(0, Qt::DisplayRole, Formatting::prettyID(subkey.keyID()));
        item->setData(0, Qt::UserRole, QVariant::fromValue(subkey));
        item->setData(1, Qt::DisplayRole, Kleo::Formatting::type(subkey));
        item->setData(2, Qt::DisplayRole, Kleo::Formatting::creationDateString(subkey));
        item->setData(3, Qt::DisplayRole, Kleo::Formatting::expirationDateString(subkey));
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
        item->setData(7, Qt::DisplayRole, subkey.keyID() == key.keyID() ? QStringLiteral("✓") : QString());
        d->ui.subkeysTree->addTopLevelItem(item);
        if (subkey.fingerprint() == selectedKeyFingerprint) {
            d->ui.subkeysTree->setCurrentItem(item);
        }
    }

    const auto subkey = key.subkey(0);
    if (const char *card = subkey.cardSerialNumber()) {
        d->ui.stored->setText(i18nc("stored...", "on SmartCard with serial no. %1", QString::fromUtf8(card)));
    } else {
        d->ui.stored->setText(i18nc("stored...", "on this computer"));
    }
    d->ui.subkeysTree->resizeColumnToContents(0);
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
    KConfigGroup dialog(KSharedConfig::openConfig(), "SubKeysDialog");
    const QSize size = dialog.readEntry("Size", QSize(820, 280));
    if (size.isValid()) {
        resize(size);
    }
}

void SubKeysDialog::writeConfig()
{
    KConfigGroup dialog(KSharedConfig::openConfig(), "SubKeysDialog");
    dialog.writeEntry("Size", size());
    dialog.sync();
}

void SubKeysDialog::setKey(const GpgME::Key &key)
{
    auto w = findChild<SubKeysWidget*>();
    Q_ASSERT(w);
    w->setKey(key);
}

GpgME::Key SubKeysDialog::key() const
{
    auto w = findChild<SubKeysWidget*>();
    Q_ASSERT(w);
    return w->key();
}
