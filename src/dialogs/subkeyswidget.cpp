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
#ifdef QGPGME_SUPPORTS_SECRET_SUBKEY_EXPORT
#include "commands/exportsecretsubkeycommand.h"
#endif
#include "commands/keytocardcommand.h"
#include "commands/importpaperkeycommand.h"
#include "exportdialog.h"

#include <Libkleo/Formatting>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDialogButtonBox>
#include <QHBoxLayout>
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

namespace
{
class SubkeysTable : public QTreeWidget
{
    Q_OBJECT
public:
    using QTreeWidget::QTreeWidget;

protected:
    QModelIndex moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override
    {
        // make keyboard navigation with Left/Right possible by switching the selection behavior to SelectItems
        // before calling QTreeWidget::moveCursor, because QTreeWidget::moveCursor ignores MoveLeft/MoveRight
        // if the selection behavior is SelectRows
        if ((cursorAction == MoveLeft) || (cursorAction == MoveRight)) {
            setSelectionBehavior(SelectItems);
        }
        const auto result = QTreeWidget::moveCursor(cursorAction, modifiers);
        if ((cursorAction == MoveLeft) || (cursorAction == MoveRight)) {
            setSelectionBehavior(SelectRows);
        }
        return result;
    }
};
}

class SubKeysWidget::Private
{
    SubKeysWidget *const q;
public:
    Private(SubKeysWidget *qq)
        : q{qq}
        , ui{qq}
    {
        ui.subkeysTree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui.subkeysTree, &QAbstractItemView::customContextMenuRequested,
                q, [this](const QPoint &p) { tableContextMenuRequested(p); });
    }

private:
    void tableContextMenuRequested(const QPoint &p);

public:
    GpgME::Key key;

public:
    struct UI {
        QVBoxLayout *mainLayout;
        SubkeysTable *subkeysTree;
        QLabel *stored;

        UI(QWidget *widget)
        {
            mainLayout = new QVBoxLayout{widget};
            mainLayout->setContentsMargins(0, 0, 0, 0);

            auto subkeysTreeLabel = new QLabel{i18nc("@label", "Subkeys:"), widget};
            mainLayout->addWidget(subkeysTreeLabel);

            subkeysTree = new SubkeysTable{widget};
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
            });
            mainLayout->addWidget(subkeysTree);

            {
                auto hbox = new QHBoxLayout;

                auto label = new QLabel{i18nc("@label", "Stored:"), widget};
                hbox->addWidget(label);

                stored = new QLabel{i18nc("@", "unknown"), widget};
                label->setBuddy(stored);
                hbox->addWidget(stored);

                hbox->addStretch();

                mainLayout->addLayout(hbox);
            }
        }
    } ui;
};

void SubKeysWidget::Private::tableContextMenuRequested(const QPoint &p)
{
    auto item = ui.subkeysTree->itemAt(p);
    if (!item) {
        return;
    }
    const auto subkey = item->data(0, Qt::UserRole).value<GpgME::Subkey>();
    const bool isOpenPGPKey = subkey.parent().protocol() == GpgME::OpenPGP;

    auto menu = new QMenu(q);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

    bool hasActions = false;

    if (isOpenPGPKey && subkey.parent().hasSecret()) {
        hasActions = true;
        menu->addAction(i18n("Change Expiry Date..."), q,
                [this, subkey]() {
                    auto cmd = new ChangeExpiryCommand(subkey.parent());
                    cmd->setSubkey(subkey);
                    ui.subkeysTree->setEnabled(false);
                    connect(cmd, &ChangeExpiryCommand::finished,
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

    if (isOpenPGPKey && subkey.canAuthenticate()) {
        hasActions = true;
        menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-export")),
                i18n("Export OpenSSH key"),
                q, [this, subkey]() {
            QScopedPointer<ExportDialog> dlg(new ExportDialog(q));
            dlg->setKey(subkey, static_cast<unsigned int> (GpgME::Context::ExportSSH));
            dlg->exec();
        });
    }

    if (!subkey.isSecret()) {
        hasActions = true;
        menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-import")),
                        i18n("Restore printed backup"),
                        q, [this, subkey] () {
            auto cmd = new ImportPaperKeyCommand(subkey.parent());
            ui.subkeysTree->setEnabled(false);
            connect(cmd, &ImportPaperKeyCommand::finished,
                    q, [this]() { ui.subkeysTree->setEnabled(true); });
            cmd->setParentWidget(q);
            cmd->start();
        });
    }

    if (subkey.isSecret()) {
        hasActions = true;
        auto action = menu->addAction(QIcon::fromTheme(QStringLiteral("send-to-symbolic")),
                                      i18n("Transfer to smartcard"),
                                      q, [this, subkey]() {
            auto cmd = new KeyToCardCommand(subkey);
            ui.subkeysTree->setEnabled(false);
            connect(cmd, &KeyToCardCommand::finished,
                    q, [this]() { ui.subkeysTree->setEnabled(true); });
            cmd->setParentWidget(q);
            cmd->start();
        });
        action->setEnabled(!KeyToCardCommand::getSuitableCards(subkey).empty());
    }

#ifdef QGPGME_SUPPORTS_SECRET_SUBKEY_EXPORT
    const bool isPrimarySubkey = subkey.keyID() == key.keyID();
    if (isOpenPGPKey && subkey.isSecret() && !isPrimarySubkey) {
        hasActions = true;
        menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-export")),
                        i18n("Export secret subkey"),
                        q, [this, subkey]() {
            auto cmd = new ExportSecretSubkeyCommand{{subkey}};
            ui.subkeysTree->setEnabled(false);
            connect(cmd, &ExportSecretSubkeyCommand::finished,
                    q, [this]() { ui.subkeysTree->setEnabled(true); });
            cmd->setParentWidget(q);
            cmd->start();
        });
    }
#endif

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

    for (const auto subkeys = key.subkeys(); const auto &subkey : subkeys) {
        auto item = new QTreeWidgetItem;
        item->setData(0, Qt::DisplayRole, Formatting::prettyID(subkey.keyID()));
        item->setData(0, Qt::AccessibleTextRole, Formatting::accessibleHexID(subkey.keyID()));
        item->setData(0, Qt::UserRole, QVariant::fromValue(subkey));
        item->setData(1, Qt::DisplayRole, Kleo::Formatting::type(subkey));
        item->setData(2, Qt::DisplayRole, Kleo::Formatting::creationDateString(subkey));
        item->setData(2, Qt::AccessibleTextRole, Formatting::accessibleCreationDate(subkey));
        item->setData(3, Qt::DisplayRole, Kleo::Formatting::expirationDateString(subkey));
        item->setData(3, Qt::AccessibleTextRole, Formatting::accessibleExpirationDate(subkey));
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
    KConfigGroup dialog(KSharedConfig::openStateConfig(), "SubKeysDialog");
    const QSize size = dialog.readEntry("Size", QSize(820, 280));
    if (size.isValid()) {
        resize(size);
    }
}

void SubKeysDialog::writeConfig()
{
    KConfigGroup dialog(KSharedConfig::openStateConfig(), "SubKeysDialog");
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

#include "subkeyswidget.moc"
