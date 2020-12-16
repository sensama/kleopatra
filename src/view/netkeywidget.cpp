/*  view/netkeywidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "netkeywidget.h"
#include "nullpinwidget.h"
#include "keytreeview.h"
#include "kleopatraapplication.h"
#include "systrayicon.h"

#include "kleopatra_debug.h"

#include "smartcard/netkeycard.h"
#include "smartcard/readerstatus.h"

#include "commands/changepincommand.h"
#include "commands/createopenpgpkeyfromcardkeyscommand.h"
#include "commands/learncardkeyscommand.h"
#include "commands/detailscommand.h"

#include <Libkleo/KeyListModel>

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QTreeView>

#include <KLocalizedString>
#include <KMessageBox>

using namespace Kleo;
using namespace Kleo::SmartCard;
using namespace Kleo::Commands;

NetKeyWidget::NetKeyWidget(QWidget *parent) :
    QWidget(parent),
    mSerialNumberLabel(new QLabel(this)),
    mVersionLabel(new QLabel(this)),
    mLearnKeysLabel(new QLabel(this)),
    mErrorLabel(new QLabel(this)),
    mNullPinWidget(new NullPinWidget()),
    mLearnKeysBtn(new QPushButton(this)),
    mChangeNKSPINBtn(new QPushButton(this)),
    mChangeSigGPINBtn(new QPushButton(this)),
    mTreeView(new KeyTreeView(this)),
    mArea(new QScrollArea)
{
    auto vLay = new QVBoxLayout;

    // Set up the scroll are
    mArea->setFrameShape(QFrame::NoFrame);
    mArea->setWidgetResizable(true);
    auto mAreaWidget = new QWidget;
    mAreaWidget->setLayout(vLay);
    mArea->setWidget(mAreaWidget);
    auto scrollLay = new QVBoxLayout(this);
    scrollLay->setContentsMargins(0, 0, 0, 0);
    scrollLay->addWidget(mArea);

    // Add general widgets
    mVersionLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    vLay->addWidget(mVersionLabel, 0, Qt::AlignLeft);

    mSerialNumberLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

    auto hLay1 = new QHBoxLayout;
    hLay1->addWidget(new QLabel(i18n("Serial number:")));
    hLay1->addWidget(mSerialNumberLabel);
    hLay1->addStretch(1);
    vLay->addLayout(hLay1);

    vLay->addWidget(mNullPinWidget);


    auto line1 = new QFrame();
    line1->setFrameShape(QFrame::HLine);
    vLay->addWidget(line1);
    vLay->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(i18n("Certificates:"))), 0, Qt::AlignLeft);

    mLearnKeysLabel = new QLabel(i18n("There are unknown certificates on this card."));
    mLearnKeysBtn->setText(i18nc("@action", "Load Certificates"));
    connect(mLearnKeysBtn, &QPushButton::clicked, this, [this] () {
        mLearnKeysBtn->setEnabled(false);
        auto cmd = new LearnCardKeysCommand(GpgME::CMS);
        cmd->setParentWidget(this);
        cmd->start();

        auto icon = KleopatraApplication::instance()->sysTrayIcon();
        if (icon) {
            icon->setLearningInProgress(true);
        }

        connect(cmd, &Command::finished, this, [icon] () {
            ReaderStatus::mutableInstance()->updateStatus();
            icon->setLearningInProgress(false);
        });
    });

    auto hLay2 = new QHBoxLayout;
    hLay2->addWidget(mLearnKeysLabel);
    hLay2->addWidget(mLearnKeysBtn);
    hLay2->addStretch(1);
    vLay->addLayout(hLay2);

    mErrorLabel->setVisible(false);
    vLay->addWidget(mErrorLabel);

    // The certificate view
    mTreeView->setHierarchicalModel(AbstractKeyListModel::createHierarchicalKeyListModel(mTreeView));
    mTreeView->setHierarchicalView(true);

    connect(mTreeView->view(), &QAbstractItemView::doubleClicked, this, [this] (const QModelIndex &idx) {
        const auto klm = dynamic_cast<KeyListModelInterface *> (mTreeView->view()->model());
        if (!klm) {
            qCDebug(KLEOPATRA_LOG) << "Unhandled Model: " << mTreeView->view()->model()->metaObject()->className();
            return;
        }
        auto cmd = new DetailsCommand(klm->key(idx), nullptr);
        cmd->setParentWidget(this);
        cmd->start();
    });
    vLay->addWidget(mTreeView);


    // The action area
    auto line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    vLay->addWidget(line2);
    vLay->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(i18n("Actions:"))), 0, Qt::AlignLeft);

    auto actionLayout = new QHBoxLayout();

    if (CreateOpenPGPKeyFromCardKeysCommand::isSupported()) {
        mKeyForCardKeysButton = new QPushButton(this);
        mKeyForCardKeysButton->setText(i18nc("@action:button", "Create OpenPGP Key"));
        mKeyForCardKeysButton->setToolTip(i18nc("@info:tooltip", "Create an OpenPGP key for the keys stored on the card."));
        actionLayout->addWidget(mKeyForCardKeysButton);
        connect(mKeyForCardKeysButton, &QPushButton::clicked, this, &NetKeyWidget::createKeyFromCardKeys);
    }

    mChangeNKSPINBtn->setText(i18nc("NKS is an identifier for a type of keys on a NetKey card", "Change NKS PIN"));
    mChangeSigGPINBtn->setText(i18nc("SigG is an identifier for a type of keys on a NetKey card", "Change SigG PIN"));

    connect(mChangeNKSPINBtn, &QPushButton::clicked, this, [this] () { doChangePin(NetKeyCard::nksPinKeyRef()); });
    connect(mChangeSigGPINBtn, &QPushButton::clicked, this, [this] () { doChangePin(NetKeyCard::sigGPinKeyRef()); });

    actionLayout->addWidget(mChangeNKSPINBtn);
    actionLayout->addWidget(mChangeSigGPINBtn);
    actionLayout->addStretch(1);

    vLay->addLayout(actionLayout);
    vLay->addStretch(1);
}

NetKeyWidget::~NetKeyWidget()
{
}

void NetKeyWidget::setCard(const NetKeyCard* card)
{
    mSerialNumber = card->serialNumber();
    mVersionLabel->setText(i18nc("1 is a Version number", "NetKey v%1 Card", card->appVersion()));
    mSerialNumberLabel->setText(QString::fromStdString(mSerialNumber));

    mNullPinWidget->setSerialNumber(mSerialNumber);
    /* According to users of NetKey Cards it is fairly uncommon
     * to use SigG Certificates at all. So it should be optional to set the pins. */
    mNullPinWidget->setVisible(card->hasNKSNullPin() /*|| card->hasSigGNullPin()*/);

    mNullPinWidget->setSigGVisible(false/*card->hasSigGNullPin()*/);
    mNullPinWidget->setNKSVisible(card->hasNKSNullPin());
    mChangeNKSPINBtn->setEnabled(!card->hasNKSNullPin());

    if (card->hasSigGNullPin()) {
        mChangeSigGPINBtn->setText(i18nc("SigG is an identifier for a type of keys on a NetKey card",
                                   "Set SigG PIN"));
    } else {
        mChangeSigGPINBtn->setText(i18nc("SigG is an identifier for a type of keys on a NetKey card",
                                  "Change SigG PIN"));
    }

    mLearnKeysBtn->setEnabled(true);
    mLearnKeysBtn->setVisible(card->canLearnKeys());
    mTreeView->setVisible(!card->canLearnKeys());
    mLearnKeysLabel->setVisible(card->canLearnKeys());

    const auto errMsg = card->errorMsg();
    if (!errMsg.isEmpty()) {
        mErrorLabel->setText(QStringLiteral("<b>%1:</b> %2").arg(i18n("Error"), errMsg));
        mErrorLabel->setVisible(true);
    } else {
        mErrorLabel->setVisible(false);
    }

    const auto keys = card->keys();
    mTreeView->setKeys(keys);

    if (mKeyForCardKeysButton) {
        mKeyForCardKeysButton->setEnabled(!card->hasNKSNullPin() && card->hasSigningKey() && card->hasEncryptionKey());
    }
}

void NetKeyWidget::doChangePin(const std::string &keyRef)
{
    auto cmd = new ChangePinCommand(mSerialNumber, NetKeyCard::AppName, this);
    this->setEnabled(false);
    connect(cmd, &ChangePinCommand::finished,
            this, [this]() {
                this->setEnabled(true);
            });
    cmd->setKeyRef(keyRef);
    cmd->start();
}

void NetKeyWidget::createKeyFromCardKeys()
{
    auto cmd = new CreateOpenPGPKeyFromCardKeysCommand(mSerialNumber, NetKeyCard::AppName, this);
    this->setEnabled(false);
    connect(cmd, &CreateOpenPGPKeyFromCardKeysCommand::finished,
            this, [this]() {
                this->setEnabled(true);
            });
    cmd->start();
}
