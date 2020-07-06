/*  Copyright (c) 2016 Klarälvdalens Datakonsult AB
    2017 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "subkeyswidget.h"
#include "ui_subkeyswidget.h"

#include "smartcard/readerstatus.h"

#include "commands/keytocardcommand.h"
#include "commands/importpaperkeycommand.h"

#include <gpgme++/key.h>

#include <KConfigGroup>
#include <KSharedConfig>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTreeWidgetItem>
#include <QMenu>

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
        if (cards.size() && cards[0]->appType() == SmartCard::Card::OpenPGPApplication) {
            const auto card = cards[0];

            if (!subkey.cardSerialNumber() || card->serialNumber() != subkey.cardSerialNumber()) {
                hasActions = true;
                menu->addAction(QIcon::fromTheme(QStringLiteral("send-to-symbolic")),
                                i18n("Transfer to smartcard"),
                                q, [this, subkey, card]() {
                    auto cmd = new Kleo::Commands::KeyToCardCommand(subkey, card->serialNumber());
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
