/*  view/pivcardwiget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pivcardwidget.h"

#include "commands/certificatetopivcardcommand.h"
#include "commands/changepincommand.h"
#include "commands/keytocardcommand.h"
#include "commands/pivgeneratecardkeycommand.h"
#include "commands/setpivcardapplicationadministrationkeycommand.h"

#include "smartcard/pivcard.h"
#include "smartcard/readerstatus.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <KMessageBox>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::SmartCard;

namespace {
static QString formatVersion(int value)
{
    if (value < 0) {
        return QLatin1String("n/a");
    }

    const unsigned int a = ((value >> 24) & 0xff);
    const unsigned int b = ((value >> 16) & 0xff);
    const unsigned int c = ((value >>  8) & 0xff);
    const unsigned int d = ((value      ) & 0xff);
    if (a) {
        return QStringLiteral("%1.%2.%3.%4").arg(QString::number(a), QString::number(b), QString::number(c), QString::number(d));
    } else if (b) {
        return QStringLiteral("%1.%2.%3").arg(QString::number(b), QString::number(c), QString::number(d));
    } else if (c) {
        return QStringLiteral("%1.%2").arg(QString::number(c), QString::number(d));
    }
    return QString::number(d);
}
} // Namespace

PIVCardWidget::PIVCardWidget(QWidget *parent):
    QWidget(parent),
    mSerialNumber(new QLabel(this)),
    mVersionLabel(new QLabel(this)),
    mPIVAuthenticationKey(new QLabel(this)),
    mCardAuthenticationKey(new QLabel(this)),
    mDigitalSignatureKey(new QLabel(this)),
    mKeyManagementKey(new QLabel(this)),
    mGeneratePIVAuthenticationKeyBtn(new QPushButton(this)),
    mWritePIVAuthenticationCertificateBtn(new QPushButton(this)),
    mGenerateCardAuthenticationKeyBtn(new QPushButton(this)),
    mWriteCardAuthenticationCertificateBtn(new QPushButton(this)),
    mGenerateDigitalSignatureKeyBtn(new QPushButton(this)),
    mWriteDigitalSignatureCertificateBtn(new QPushButton(this)),
    mGenerateKeyManagementKeyBtn(new QPushButton(this)),
    mWriteKeyManagementCertificateBtn(new QPushButton(this)),
    mWriteKeyManagementKeyBtn(new QPushButton(this))
{
    auto grid = new QGridLayout;
    int row = 0;

    // Set up the scroll are
    auto area = new QScrollArea;
    area->setFrameShape(QFrame::NoFrame);
    area->setWidgetResizable(true);
    auto areaWidget = new QWidget;
    auto areaVLay = new QVBoxLayout(areaWidget);
    areaVLay->addLayout(grid);
    areaVLay->addStretch(1);
    area->setWidget(areaWidget);
    auto myLayout = new QVBoxLayout(this);
    myLayout->addWidget(area);

    // Version and Serialnumber
    grid->addWidget(mVersionLabel, row++, 0, 1, 2);
    mVersionLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    grid->addWidget(new QLabel(i18n("Serial number:")), row, 0);

    grid->addWidget(mSerialNumber, row++, 1);
    mSerialNumber->setTextInteractionFlags(Qt::TextBrowserInteraction);

    // The keys
    auto line1 = new QFrame();
    line1->setFrameShape(QFrame::HLine);
    grid->addWidget(line1, row++, 0, 1, 5);
    grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(i18n("Keys:"))), row++, 0);

    grid->addWidget(new QLabel(i18n("PIV authentication:")), row, 0);
    grid->addWidget(mPIVAuthenticationKey, row, 1);
    mPIVAuthenticationKey->setTextInteractionFlags(Qt::TextBrowserInteraction);
    mGeneratePIVAuthenticationKeyBtn->setText(i18nc("@action:button", "Generate"));
    mGeneratePIVAuthenticationKeyBtn->setEnabled(false);
    grid->addWidget(mGeneratePIVAuthenticationKeyBtn, row, 2);
    connect(mGeneratePIVAuthenticationKeyBtn, &QPushButton::clicked, this, [this] () { generateKey(PIVCard::pivAuthenticationKeyRef()); });
    mWritePIVAuthenticationCertificateBtn->setText(i18nc("@action:button", "Write Certificate"));
    mWritePIVAuthenticationCertificateBtn->setToolTip(i18nc("@info:tooltip", "Write the certificate corresponding to this key to the card"));
    mWritePIVAuthenticationCertificateBtn->setEnabled(false);
    grid->addWidget(mWritePIVAuthenticationCertificateBtn, row, 3);
    connect(mWritePIVAuthenticationCertificateBtn, &QPushButton::clicked, this, [this] () { writeCertificateToCard(PIVCard::pivAuthenticationKeyRef()); });
    row++;

    grid->addWidget(new QLabel(i18n("Card authentication:")), row, 0);
    grid->addWidget(mCardAuthenticationKey, row, 1);
    mCardAuthenticationKey->setTextInteractionFlags(Qt::TextBrowserInteraction);
    mGenerateCardAuthenticationKeyBtn->setText(i18nc("@action:button", "Generate"));
    mGenerateCardAuthenticationKeyBtn->setEnabled(false);
    grid->addWidget(mGenerateCardAuthenticationKeyBtn, row, 2);
    connect(mGenerateCardAuthenticationKeyBtn, &QPushButton::clicked, this, [this] () { generateKey(PIVCard::cardAuthenticationKeyRef()); });
    mWriteCardAuthenticationCertificateBtn->setText(i18nc("@action:button", "Write Certificate"));
    mWriteCardAuthenticationCertificateBtn->setToolTip(i18nc("@info:tooltip", "Write the certificate corresponding to this key to the card"));
    mWriteCardAuthenticationCertificateBtn->setEnabled(false);
    grid->addWidget(mWriteCardAuthenticationCertificateBtn, row, 3);
    connect(mWriteCardAuthenticationCertificateBtn, &QPushButton::clicked, this, [this] () { writeCertificateToCard(PIVCard::cardAuthenticationKeyRef()); });
    row++;

    grid->addWidget(new QLabel(i18n("Digital signature:")), row, 0);
    grid->addWidget(mDigitalSignatureKey, row, 1);
    mDigitalSignatureKey->setTextInteractionFlags(Qt::TextBrowserInteraction);
    mGenerateDigitalSignatureKeyBtn->setText(i18nc("@action:button", "Generate"));
    mGenerateDigitalSignatureKeyBtn->setEnabled(false);
    grid->addWidget(mGenerateDigitalSignatureKeyBtn, row, 2);
    connect(mGenerateDigitalSignatureKeyBtn, &QPushButton::clicked, this, [this] () { generateKey(PIVCard::digitalSignatureKeyRef()); });
    mWriteDigitalSignatureCertificateBtn->setText(i18nc("@action:button", "Write Certificate"));
    mWriteDigitalSignatureCertificateBtn->setToolTip(i18nc("@info:tooltip", "Write the certificate corresponding to this key to the card"));
    mWriteDigitalSignatureCertificateBtn->setEnabled(false);
    grid->addWidget(mWriteDigitalSignatureCertificateBtn, row, 3);
    connect(mWriteDigitalSignatureCertificateBtn, &QPushButton::clicked, this, [this] () { writeCertificateToCard(PIVCard::digitalSignatureKeyRef()); });
    row++;

    grid->addWidget(new QLabel(i18n("Key management:")), row, 0);
    grid->addWidget(mKeyManagementKey, row, 1);
    mKeyManagementKey->setTextInteractionFlags(Qt::TextBrowserInteraction);
    mGenerateKeyManagementKeyBtn->setText(i18nc("@action:button", "Generate"));
    mGenerateKeyManagementKeyBtn->setEnabled(false);
    grid->addWidget(mGenerateKeyManagementKeyBtn, row, 2);
    connect(mGenerateKeyManagementKeyBtn, &QPushButton::clicked, this, [this] () { generateKey(PIVCard::keyManagementKeyRef()); });
    mWriteKeyManagementCertificateBtn->setText(i18nc("@action:button", "Write Certificate"));
    mWriteKeyManagementCertificateBtn->setToolTip(i18nc("@info:tooltip", "Write the certificate corresponding to this key to the card"));
    mWriteKeyManagementCertificateBtn->setEnabled(false);
    grid->addWidget(mWriteKeyManagementCertificateBtn, row, 3);
    connect(mWriteKeyManagementCertificateBtn, &QPushButton::clicked, this, [this] () { writeCertificateToCard(PIVCard::keyManagementKeyRef()); });
    mWriteKeyManagementKeyBtn->setText(i18nc("@action:button", "Write Key"));
    mWriteKeyManagementKeyBtn->setToolTip(i18nc("@info:tooltip", "Write the key pair of a certificate to the card"));
    mWriteKeyManagementKeyBtn->setEnabled(true);
    grid->addWidget(mWriteKeyManagementKeyBtn, row, 4);
    connect(mWriteKeyManagementKeyBtn, &QPushButton::clicked, this, [this] () { writeKeyToCard(PIVCard::keyManagementKeyRef()); });
    row++;

    auto line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    grid->addWidget(line2, row++, 0, 1, 5);

    auto actionLayout = new QHBoxLayout;

    {
        auto button = new QPushButton(i18nc("@action:button", "Change PIN"));
        button->setToolTip(i18nc("@info:tooltip", "Change the PIV Card Application PIN that activates the PIV Card "
                                 "and enables private key operations using the stored keys."));
        actionLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this] () { changePin(PIVCard::pinKeyRef()); });
    }
    {
        auto button = new QPushButton(i18nc("@action:button", "Change PUK"));
        button->setToolTip(i18nc("@info:tooltip", "Change the PIN Unblocking Key that enables a reset of the PIN."));
        actionLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this] () { changePin(PIVCard::pukKeyRef()); });
    }
    {
        auto button = new QPushButton(i18nc("@action:button", "Change Admin Key"));
        button->setToolTip(i18nc("@info:tooltip", "Change the PIV Card Application Administration Key that is used by the "
                                 "PIV Card Application to authenticate the PIV Card Application Administrator and by the "
                                 "administrator (resp. Kleopatra) to authenticate the PIV Card Application."));
        actionLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this] () { setAdminKey(); });
    }

    actionLayout->addStretch(-1);
    grid->addLayout(actionLayout, row++, 0, 1, 4);

    grid->setColumnStretch(4, -1);
}

PIVCardWidget::~PIVCardWidget()
{
}

void PIVCardWidget::setCard(const PIVCard *card)
{
    mCardSerialNumber = card->serialNumber();
    mVersionLabel->setText(i18nc("%1 version number", "PIV v%1 card", formatVersion(card->appVersion())));

    if (card->displaySerialNumber() != card->serialNumber()) {
        mSerialNumber->setText(QStringLiteral("%1 (%2)").arg(QString::fromStdString(card->displaySerialNumber()),
                                                             QString::fromStdString(card->serialNumber())));
    } else {
        mSerialNumber->setText(QString::fromStdString(card->serialNumber()));
    }

    updateKey(PIVCard::pivAuthenticationKeyRef(), card, mPIVAuthenticationKey, mGeneratePIVAuthenticationKeyBtn, mWritePIVAuthenticationCertificateBtn);
    updateKey(PIVCard::cardAuthenticationKeyRef(), card, mCardAuthenticationKey, mGenerateCardAuthenticationKeyBtn, mWriteCardAuthenticationCertificateBtn);
    updateKey(PIVCard::digitalSignatureKeyRef(), card, mDigitalSignatureKey, mGenerateDigitalSignatureKeyBtn, mWriteDigitalSignatureCertificateBtn);
    updateKey(PIVCard::keyManagementKeyRef(), card, mKeyManagementKey, mGenerateKeyManagementKeyBtn, mWriteKeyManagementCertificateBtn);
}

void PIVCardWidget::updateKey(const std::string &keyRef, const PIVCard *card, QLabel *label, QPushButton *generateButton, QPushButton *writeButton)
{
    const std::string grip = card->keyGrip(keyRef);
    label->setText(grip.empty() ? i18nc("@info", "Slot empty") : QString::fromStdString(grip));
    generateButton->setText(grip.empty() ? i18nc("@action:button", "Generate") : i18nc("@action:button", "Replace"));
    generateButton->setToolTip(grip.empty() ?
        i18nc("@info:tooltip %1 display name of a key", "Generate %1", PIVCard::keyDisplayName(keyRef)) :
        i18nc("@info:tooltip %1 display name of a key", "Replace %1 with new key", PIVCard::keyDisplayName(keyRef)));
    generateButton->setEnabled(true);
    if (writeButton) {
        writeButton->setEnabled(!grip.empty());
    }
}

void PIVCardWidget::generateKey(const std::string &keyref)
{
    auto cmd = new PIVGenerateCardKeyCommand(mCardSerialNumber, this);
    this->setEnabled(false);
    connect(cmd, &PIVGenerateCardKeyCommand::finished,
            this, [this]() {
                this->setEnabled(true);
            });
    cmd->setKeyRef(keyref);
    cmd->start();
}

void PIVCardWidget::writeCertificateToCard(const std::string &keyref)
{
    auto cmd = new CertificateToPIVCardCommand(keyref, mCardSerialNumber);
    this->setEnabled(false);
    connect(cmd, &CertificateToPIVCardCommand::finished,
            this, [this]() {
                this->setEnabled(true);
            });
    cmd->setParentWidget(this);
    cmd->start();
}

void PIVCardWidget::writeKeyToCard(const std::string &keyref)
{
    auto cmd = new KeyToCardCommand(keyref, mCardSerialNumber);
    this->setEnabled(false);
    connect(cmd, &KeyToCardCommand::finished,
            this, [this]() {
                this->setEnabled(true);
            });
    cmd->setParentWidget(this);
    cmd->start();
}

void PIVCardWidget::changePin(const std::string &keyRef)
{
    auto cmd = new ChangePinCommand(mCardSerialNumber, this);
    this->setEnabled(false);
    connect(cmd, &ChangePinCommand::finished,
            this, [this]() {
                this->setEnabled(true);
            });
    cmd->setKeyRef(keyRef);
    cmd->start();
}

void PIVCardWidget::setAdminKey()
{
    auto cmd = new SetPIVCardApplicationAdministrationKeyCommand(mCardSerialNumber, this);
    this->setEnabled(false);
    connect(cmd, &SetPIVCardApplicationAdministrationKeyCommand::finished,
            this, [this]() {
                this->setEnabled(true);
            });
    cmd->start();
}
