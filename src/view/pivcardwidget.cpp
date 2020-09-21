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
}

PIVCardWidget::PIVCardWidget(QWidget *parent):
    QWidget(parent),
    mSerialNumber(new QLabel(this)),
    mVersionLabel(new QLabel(this))
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
    grid->addWidget(line1, row++, 0, 1, 6);
    grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(i18n("Keys:"))), row++, 0);

    mPIVAuthenticationKey = createKeyWidgets(PIVCard::pivAuthenticationKeyRef());
    grid->addWidget(new QLabel(i18n("PIV authentication:")), row, 0);
    grid->addWidget(mPIVAuthenticationKey.keyGrip, row, 1);
    grid->addWidget(mPIVAuthenticationKey.keyAlgorithm, row, 2);
    grid->addWidget(mPIVAuthenticationKey.generateButton, row, 3);
    grid->addWidget(mPIVAuthenticationKey.writeCertificateButton, row, 4);
    row++;

    mCardAuthenticationKey = createKeyWidgets(PIVCard::cardAuthenticationKeyRef());
    grid->addWidget(new QLabel(i18n("Card authentication:")), row, 0);
    grid->addWidget(mCardAuthenticationKey.keyGrip, row, 1);
    grid->addWidget(mCardAuthenticationKey.keyAlgorithm, row, 2);
    grid->addWidget(mCardAuthenticationKey.generateButton, row, 3);
    grid->addWidget(mCardAuthenticationKey.writeCertificateButton, row, 4);
    row++;

    mDigitalSignatureKey = createKeyWidgets(PIVCard::digitalSignatureKeyRef());
    grid->addWidget(new QLabel(i18n("Digital signature:")), row, 0);
    grid->addWidget(mDigitalSignatureKey.keyGrip, row, 1);
    grid->addWidget(mDigitalSignatureKey.keyAlgorithm, row, 2);
    grid->addWidget(mDigitalSignatureKey.generateButton, row, 3);
    grid->addWidget(mDigitalSignatureKey.writeCertificateButton, row, 4);
    row++;

    mKeyManagementKey = createKeyWidgets(PIVCard::keyManagementKeyRef());
    grid->addWidget(new QLabel(i18n("Key management:")), row, 0);
    grid->addWidget(mKeyManagementKey.keyGrip, row, 1);
    grid->addWidget(mKeyManagementKey.keyAlgorithm, row, 2);
    grid->addWidget(mKeyManagementKey.generateButton, row, 3);
    grid->addWidget(mKeyManagementKey.writeCertificateButton, row, 4);
    grid->addWidget(mKeyManagementKey.writeKeyButton, row, 5);
    row++;

    auto line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    grid->addWidget(line2, row++, 0, 1, 6);

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
    grid->addLayout(actionLayout, row++, 0, 1, 6);

    grid->setColumnStretch(4, -1);
}

PIVCardWidget::KeyWidgets PIVCardWidget::createKeyWidgets(const std::string &keyRef)
{
    KeyWidgets keyWidgets;
    keyWidgets.keyGrip = new QLabel(this);
    keyWidgets.keyGrip->setTextInteractionFlags(Qt::TextBrowserInteraction);
    keyWidgets.keyAlgorithm = new QLabel(this);
    keyWidgets.generateButton = new QPushButton(i18nc("@action:button", "Generate"), this);
    keyWidgets.generateButton->setEnabled(false);
    connect(keyWidgets.generateButton, &QPushButton::clicked, this, [this, keyRef] () { generateKey(keyRef); });
    keyWidgets.writeCertificateButton = new QPushButton(i18nc("@action:button", "Write Certificate"));
    keyWidgets.writeCertificateButton->setToolTip(i18nc("@info:tooltip", "Write the certificate corresponding to this key to the card"));
    keyWidgets.writeCertificateButton->setEnabled(false);
    connect(keyWidgets.writeCertificateButton, &QPushButton::clicked, this, [this, keyRef] () { writeCertificateToCard(keyRef); });
    if (keyRef == PIVCard::keyManagementKeyRef()) {
        keyWidgets.writeKeyButton = new QPushButton(i18nc("@action:button", "Write Key"));
        keyWidgets.writeKeyButton->setToolTip(i18nc("@info:tooltip", "Write the key pair of a certificate to the card"));
        keyWidgets.writeKeyButton->setEnabled(true);
        connect(keyWidgets.writeKeyButton, &QPushButton::clicked, this, [this, keyRef] () { writeKeyToCard(keyRef); });
    }
    return keyWidgets;
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

    updateKey(PIVCard::pivAuthenticationKeyRef(), card, mPIVAuthenticationKey);
    updateKey(PIVCard::cardAuthenticationKeyRef(), card, mCardAuthenticationKey);
    updateKey(PIVCard::digitalSignatureKeyRef(), card, mDigitalSignatureKey);
    updateKey(PIVCard::keyManagementKeyRef(), card, mKeyManagementKey);
}

void PIVCardWidget::updateKey(const std::string &keyRef, const PIVCard *card, const KeyWidgets &widgets)
{
    const std::string grip = card->keyGrip(keyRef);
    if (grip.empty()) {
        widgets.keyGrip->setText(i18nc("@info", "Slot empty"));
        widgets.keyAlgorithm->setText(QString());
        widgets.generateButton->setText(i18nc("@action:button", "Generate"));
        widgets.generateButton->setToolTip(
            i18nc("@info:tooltip %1 display name of a key", "Generate %1", PIVCard::keyDisplayName(keyRef)));
    } else {
        widgets.keyGrip->setText(QString::fromStdString(grip));
        const std::string algo = card->keyAlgorithm(keyRef);
        widgets.keyAlgorithm->setText(algo.empty() ? i18nc("@info unknown key algorithm", "unknown") : QString::fromStdString(algo));
        widgets.generateButton->setText(i18nc("@action:button", "Replace"));
        widgets.generateButton->setToolTip(
            i18nc("@info:tooltip %1 display name of a key", "Replace %1 with new key", PIVCard::keyDisplayName(keyRef)));
    }
    widgets.generateButton->setEnabled(true);
    if (widgets.writeCertificateButton) {
        widgets.writeCertificateButton->setEnabled(!grip.empty());
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
