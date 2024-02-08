/*  view/netkeywidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "netkeywidget.h"

#include "keytreeview.h"
#include "kleopatraapplication.h"
#include "nullpinwidget.h"
#include "systrayicon.h"

#include "kleopatra_debug.h"

#include "smartcard/netkeycard.h"
#include "smartcard/readerstatus.h"

#include "commands/changepincommand.h"
#include "commands/createcsrforcardkeycommand.h"
#include "commands/createopenpgpkeyfromcardkeyscommand.h"
#include "commands/detailscommand.h"
#include "commands/learncardkeyscommand.h"

#include <Libkleo/Algorithm>
#include <Libkleo/Compliance>
#include <Libkleo/KeyListModel>

#include <KConfigGroup>
#include <KSharedConfig>

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTreeView>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <KMessageBox>

#include <gpgme++/context.h>
#include <gpgme++/engineinfo.h>

using namespace Kleo;
using namespace Kleo::SmartCard;
using namespace Kleo::Commands;

NetKeyWidget::NetKeyWidget(QWidget *parent)
    : QWidget(parent)
    , mSerialNumberLabel(new QLabel(this))
    , mVersionLabel(new QLabel(this))
    , mErrorLabel(new QLabel(this))
    , mNullPinWidget(new NullPinWidget(this))
    , mChangeNKSPINBtn(new QPushButton(this))
    , mChangeSigGPINBtn(new QPushButton(this))
    , mTreeView(new KeyTreeView(this))
    , mArea(new QScrollArea)
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

    mErrorLabel->setVisible(false);
    vLay->addWidget(mErrorLabel);

    // The certificate view
    mTreeView->setHierarchicalModel(AbstractKeyListModel::createHierarchicalKeyListModel(mTreeView));
    mTreeView->setHierarchicalView(true);

    connect(mTreeView->view(), &QAbstractItemView::doubleClicked, this, [this](const QModelIndex &idx) {
        const auto klm = dynamic_cast<KeyListModelInterface *>(mTreeView->view()->model());
        if (!klm) {
            qCDebug(KLEOPATRA_LOG) << "Unhandled Model: " << mTreeView->view()->model()->metaObject()->className();
            return;
        }
        auto cmd = new DetailsCommand(klm->key(idx));
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

    if (!(engineInfo(GpgME::GpgSMEngine).engineVersion() < "2.2.26")) { // see https://dev.gnupg.org/T5184
        mCreateCSRButton = new QPushButton(this);
        mCreateCSRButton->setText(i18nc("@action:button", "Create CSR"));
        mCreateCSRButton->setToolTip(i18nc("@info:tooltip", "Create a certificate signing request for a key stored on the card."));
        mCreateCSRButton->setEnabled(false);
        actionLayout->addWidget(mCreateCSRButton);
        connect(mCreateCSRButton, &QPushButton::clicked, this, [this]() {
            createCSR();
        });
    }

    mChangeNKSPINBtn->setText(i18nc("NKS is an identifier for a type of keys on a NetKey card", "Change NKS PIN"));
    mChangeSigGPINBtn->setText(i18nc("SigG is an identifier for a type of keys on a NetKey card", "Change SigG PIN"));

    connect(mChangeNKSPINBtn, &QPushButton::clicked, this, [this]() {
        doChangePin(NetKeyCard::nksPinKeyRef());
    });
    connect(mChangeSigGPINBtn, &QPushButton::clicked, this, [this]() {
        doChangePin(NetKeyCard::sigGPinKeyRef());
    });

    actionLayout->addWidget(mChangeNKSPINBtn);
    actionLayout->addWidget(mChangeSigGPINBtn);
    actionLayout->addStretch(1);

    vLay->addLayout(actionLayout);
    vLay->addStretch(1);

    const KConfigGroup configGroup(KSharedConfig::openConfig(), "NetKeyCardView");
    mTreeView->restoreLayout(configGroup);
}

NetKeyWidget::~NetKeyWidget() = default;

namespace
{
std::vector<KeyPairInfo> getKeysSuitableForCSRCreation(const NetKeyCard *netKeyCard)
{
    if (netKeyCard->hasNKSNullPin()) {
        return {};
    }

    std::vector<KeyPairInfo> keys;
    Kleo::copy_if(netKeyCard->keyInfos(), std::back_inserter(keys), [](const auto &keyInfo) {
        if (keyInfo.keyRef.substr(0, 9) == "NKS-SIGG.") {
            // SigG certificates for qualified signatures are issued with the physical cards;
            // it's not possible to request a certificate for them
            return false;
        }
        return keyInfo.canSign() //
            && (keyInfo.keyRef.substr(0, 9) == "NKS-NKS3.") //
            && DeVSCompliance::algorithmIsCompliant(keyInfo.algorithm);
    });

    return keys;
}
}

void NetKeyWidget::setCard(const NetKeyCard *card)
{
    mSerialNumber = card->serialNumber();
    mVersionLabel->setText(i18nc("1 is a Version number", "NetKey v%1 Card", card->appVersion()));
    mSerialNumberLabel->setText(card->displaySerialNumber());

    mNullPinWidget->setSerialNumber(mSerialNumber);
    /* According to users of NetKey Cards it is fairly uncommon
     * to use SigG Certificates at all. So it should be optional to set the pins. */
    mNullPinWidget->setVisible(card->hasNKSNullPin() /*|| card->hasSigGNullPin()*/);

    mNullPinWidget->setSigGVisible(false /*card->hasSigGNullPin()*/);
    mNullPinWidget->setNKSVisible(card->hasNKSNullPin());
    mChangeNKSPINBtn->setEnabled(!card->hasNKSNullPin());

    if (card->hasSigGNullPin()) {
        mChangeSigGPINBtn->setText(i18nc("SigG is an identifier for a type of keys on a NetKey card", "Set SigG PIN"));
    } else {
        mChangeSigGPINBtn->setText(i18nc("SigG is an identifier for a type of keys on a NetKey card", "Change SigG PIN"));
    }

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
        mKeyForCardKeysButton->setEnabled(!card->hasNKSNullPin() && card->hasSigningKey() && card->hasEncryptionKey()
                                          && DeVSCompliance::algorithmIsCompliant(card->keyInfo(card->signingKeyRef()).algorithm)
                                          && DeVSCompliance::algorithmIsCompliant(card->keyInfo(card->encryptionKeyRef()).algorithm));
    }
    if (mCreateCSRButton) {
        mCreateCSRButton->setEnabled(!getKeysSuitableForCSRCreation(card).empty());
    }

    if (card->keyInfos().size() > keys.size()) {
        // the card contains keys we don't know; try to learn them from the card
        learnCard();
    }
}

void NetKeyWidget::learnCard()
{
    auto cmd = new LearnCardKeysCommand(GpgME::CMS);
    cmd->setParentWidget(this);
    cmd->setShowsOutputWindow(false);
    cmd->start();
}

void NetKeyWidget::doChangePin(const std::string &keyRef)
{
    const auto netKeyCard = ReaderStatus::instance()->getCard<NetKeyCard>(mSerialNumber);
    if (!netKeyCard) {
        KMessageBox::error(this, i18n("Failed to find the smartcard with the serial number: %1", QString::fromStdString(mSerialNumber)));
        return;
    }

    auto cmd = new ChangePinCommand(mSerialNumber, NetKeyCard::AppName, this);
    this->setEnabled(false);
    connect(cmd, &ChangePinCommand::finished, this, [this]() {
        this->setEnabled(true);
    });
    cmd->setKeyRef(keyRef);
    if ((keyRef == NetKeyCard::nksPinKeyRef() && netKeyCard->hasNKSNullPin()) //
        || (keyRef == NetKeyCard::sigGPinKeyRef() && netKeyCard->hasSigGNullPin())) {
        cmd->setMode(ChangePinCommand::NullPinMode);
    }
    cmd->start();
}

void NetKeyWidget::createKeyFromCardKeys()
{
    auto cmd = new CreateOpenPGPKeyFromCardKeysCommand(mSerialNumber, NetKeyCard::AppName, this);
    this->setEnabled(false);
    connect(cmd, &CreateOpenPGPKeyFromCardKeysCommand::finished, this, [this]() {
        this->setEnabled(true);
    });
    cmd->start();
}

namespace
{
std::string getKeyRef(const std::vector<KeyPairInfo> &keys, QWidget *parent)
{
    QStringList options;
    for (const auto &key : keys) {
        options << QStringLiteral("%1 - %2").arg(QString::fromStdString(key.keyRef), QString::fromStdString(key.grip));
    }

    bool ok;
    const QString choice = QInputDialog::getItem(parent,
                                                 i18n("Select Key"),
                                                 i18n("Please select the key you want to create a certificate signing request for:"),
                                                 options,
                                                 /* current= */ 0,
                                                 /* editable= */ false,
                                                 &ok);
    return ok ? keys[options.indexOf(choice)].keyRef : std::string();
}
}

void NetKeyWidget::createCSR()
{
    const auto netKeyCard = ReaderStatus::instance()->getCard<NetKeyCard>(mSerialNumber);
    if (!netKeyCard) {
        KMessageBox::error(this, i18n("Failed to find the smartcard with the serial number: %1", QString::fromStdString(mSerialNumber)));
        return;
    }
    const auto suitableKeys = getKeysSuitableForCSRCreation(netKeyCard.get());
    if (suitableKeys.empty()) {
        KMessageBox::error(this, i18n("Sorry! No keys suitable for creating a certificate signing request found on the smartcard."));
        return;
    }
    const auto keyRef = getKeyRef(suitableKeys, this);
    if (keyRef.empty()) {
        return;
    }
    auto cmd = new CreateCSRForCardKeyCommand(keyRef, mSerialNumber, NetKeyCard::AppName, this);
    this->setEnabled(false);
    connect(cmd, &CreateCSRForCardKeyCommand::finished, this, [this]() {
        this->setEnabled(true);
    });
    cmd->start();
}
