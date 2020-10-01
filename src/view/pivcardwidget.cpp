/*  view/pivcardwiget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pivcardwidget.h"

#include "tooltippreferences.h"

#include "commands/certificatetopivcardcommand.h"
#include "commands/changepincommand.h"
#include "commands/importcertificatefrompivcardcommand.h"
#include "commands/keytocardcommand.h"
#include "commands/pivgeneratecardkeycommand.h"
#include "commands/setpivcardapplicationadministrationkeycommand.h"

#include "smartcard/pivcard.h"
#include "smartcard/readerstatus.h"

#include <Libkleo/Dn>
#include <Libkleo/Formatting>
#include <Libkleo/KeyCache>

#include <KLocalizedString>
#include <KMessageBox>

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

using namespace GpgME;
using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::SmartCard;

namespace {
static void layoutKeyWidgets(QGridLayout *grid, const QString &keyName, const PIVCardWidget::KeyWidgets &keyWidgets)
{
    int row = grid->rowCount();
    grid->addWidget(new QLabel(keyName), row, 0);
    grid->addWidget(keyWidgets.certificateInfo, row, 1, 1, 2);
    grid->addWidget(keyWidgets.generateButton, row, 3);
    if (keyWidgets.writeKeyButton) {
        grid->addWidget(keyWidgets.writeKeyButton, row, 4);
    }
    row++;
    grid->addWidget(keyWidgets.keyGrip, row, 1);
    grid->addWidget(keyWidgets.keyAlgorithm, row, 2);
    grid->addWidget(keyWidgets.writeCertificateButton, row, 3);
    grid->addWidget(keyWidgets.importCertificateButton, row, 4);
}

static int toolTipOptions()
{
    using namespace Kleo::Formatting;
    static const int validityFlags = Validity | Issuer | ExpiryDates | CertificateUsage;
    static const int ownerFlags = Subject | UserIDs | OwnerTrust;
    static const int detailsFlags = StorageLocation | CertificateType | SerialNumber | Fingerprint;

    const TooltipPreferences prefs;

    int flags = KeyID;
    flags |= prefs.showValidity() ? validityFlags : 0;
    flags |= prefs.showOwnerInformation() ? ownerFlags : 0;
    flags |= prefs.showCertificateDetails() ? detailsFlags : 0;
    return flags;
}
}

PIVCardWidget::PIVCardWidget(QWidget *parent):
    QWidget(parent),
    mSerialNumber(new QLabel(this)),
    mVersionLabel(new QLabel(this))
{
    // Set up the scroll area
    auto myLayout = new QVBoxLayout(this);

    auto area = new QScrollArea;
    area->setFrameShape(QFrame::NoFrame);
    area->setWidgetResizable(true);
    myLayout->addWidget(area);

    auto areaWidget = new QWidget;
    area->setWidget(areaWidget);

    auto areaVLay = new QVBoxLayout(areaWidget);

    auto grid = new QGridLayout;
    areaVLay->addLayout(grid);
    areaVLay->addStretch(1);

    const int columnCount = 5;
    int row = 0;

    // Version and Serialnumber
    grid->addWidget(mVersionLabel, row++, 0, 1, 2);
    mVersionLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

    grid->addWidget(new QLabel(i18n("Serial number:")), row, 0);
    grid->addWidget(mSerialNumber, row++, 1);
    mSerialNumber->setTextInteractionFlags(Qt::TextBrowserInteraction);

    {
        auto line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        grid->addWidget(line, row++, 0, 1, columnCount);
    }

    // The keys
    grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(i18n("Keys:"))), row++, 0);

    {
        KeyWidgets keyWidgets = createKeyWidgets(PIVCard::pivAuthenticationKeyRef());
        layoutKeyWidgets(grid, i18n("PIV authentication:"), keyWidgets);
    }
    {
        KeyWidgets keyWidgets = createKeyWidgets(PIVCard::cardAuthenticationKeyRef());
        layoutKeyWidgets(grid, i18n("Card authentication:"), keyWidgets);
    }
    {
        KeyWidgets keyWidgets = createKeyWidgets(PIVCard::digitalSignatureKeyRef());
        layoutKeyWidgets(grid, i18n("Digital signature:"), keyWidgets);
    }
    {
        KeyWidgets keyWidgets = createKeyWidgets(PIVCard::keyManagementKeyRef());
        layoutKeyWidgets(grid, i18n("Key management:"), keyWidgets);
    }
    row = grid->rowCount();

    {
        auto line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        grid->addWidget(line, row++, 0, 1, columnCount);
    }

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
    grid->addLayout(actionLayout, row++, 0, 1, columnCount);

    grid->setColumnStretch(4, -1);
}

PIVCardWidget::KeyWidgets PIVCardWidget::createKeyWidgets(const std::string &keyRef)
{
    KeyWidgets keyWidgets;
    keyWidgets.keyGrip = new QLabel(this);
    keyWidgets.keyGrip->setTextInteractionFlags(Qt::TextBrowserInteraction);
    keyWidgets.keyAlgorithm = new QLabel(this);
    keyWidgets.keyAlgorithm->setTextInteractionFlags(Qt::TextSelectableByMouse);
    keyWidgets.certificateInfo = new QLabel(this);
    keyWidgets.certificateInfo->setTextInteractionFlags(Qt::TextBrowserInteraction);
    keyWidgets.generateButton = new QPushButton(i18nc("@action:button", "Generate"), this);
    keyWidgets.generateButton->setEnabled(false);
    connect(keyWidgets.generateButton, &QPushButton::clicked,
            this, [this, keyRef] () { generateKey(keyRef); });
    keyWidgets.writeCertificateButton = new QPushButton(i18nc("@action:button", "Write Certificate"));
    keyWidgets.writeCertificateButton->setToolTip(i18nc("@info:tooltip", "Write the certificate corresponding to this key to the card"));
    keyWidgets.writeCertificateButton->setEnabled(false);
    connect(keyWidgets.writeCertificateButton, &QPushButton::clicked,
            this, [this, keyRef] () { writeCertificateToCard(keyRef); });
    keyWidgets.importCertificateButton = new QPushButton(i18nc("@action:button", "Import Certificate"));
    keyWidgets.importCertificateButton->setToolTip(i18nc("@info:tooltip", "Import the certificate stored on the card"));
    keyWidgets.importCertificateButton->setEnabled(false);
    connect(keyWidgets.importCertificateButton, &QPushButton::clicked,
            this, [this, keyRef] () { importCertificateFromCard(keyRef); });
    if (keyRef == PIVCard::keyManagementKeyRef()) {
        keyWidgets.writeKeyButton = new QPushButton(i18nc("@action:button", "Write Key"));
        keyWidgets.writeKeyButton->setToolTip(i18nc("@info:tooltip", "Write the key pair of a certificate to the card"));
        keyWidgets.writeKeyButton->setEnabled(true);
        connect(keyWidgets.writeKeyButton, &QPushButton::clicked,
                this, [this, keyRef] () { writeKeyToCard(keyRef); });
    }
    mKeyWidgets.insert(keyRef, keyWidgets);
    return keyWidgets;
}

PIVCardWidget::~PIVCardWidget()
{
}

void PIVCardWidget::setCard(const PIVCard *card)
{
    mCardSerialNumber = card->serialNumber();
    mVersionLabel->setText(i18nc("%1 version number", "PIV v%1 card", card->displayAppVersion()));

    if (card->displaySerialNumber() != card->serialNumber()) {
        mSerialNumber->setText(i18nc("%1 nice serial number, %2 real serial number", "%1 (%2)",
                                     QString::fromStdString(card->displaySerialNumber()),
                                     QString::fromStdString(card->serialNumber())));
    } else {
        mSerialNumber->setText(QString::fromStdString(card->serialNumber()));
    }

    updateKeyWidgets(PIVCard::pivAuthenticationKeyRef(), card);
    updateKeyWidgets(PIVCard::cardAuthenticationKeyRef(), card);
    updateKeyWidgets(PIVCard::digitalSignatureKeyRef(), card);
    updateKeyWidgets(PIVCard::keyManagementKeyRef(), card);
}

void PIVCardWidget::updateKeyWidgets(const std::string &keyRef, const PIVCard *card)
{
    KeyWidgets widgets = mKeyWidgets.value(keyRef);
    const std::string grip = card ? card->keyGrip(keyRef) : widgets.keyGrip->text().toStdString();
    if (grip.empty()) {
        widgets.certificateInfo->setText(i18nc("@info", "<em>slot empty</em>"));
        widgets.certificateInfo->setToolTip(QString());
        widgets.keyGrip->setText(QString());
        widgets.keyAlgorithm->setText(QString());
        widgets.generateButton->setText(i18nc("@action:button", "Generate"));
        widgets.generateButton->setToolTip(
            i18nc("@info:tooltip %1 display name of a key", "Generate %1", PIVCard::keyDisplayName(keyRef)));
        widgets.writeCertificateButton->setEnabled(false);
        widgets.importCertificateButton->setEnabled(false);
    } else {
        const Key certificate = KeyCache::instance()->findSubkeyByKeyGrip(grip).parent();
        if (!certificate.isNull() && certificate.protocol() == GpgME::CMS) {
            widgets.certificateInfo->setText(
                i18nc("X.509 certificate DN (validity, created: date)", "%1 (%2, created: %3)",
                      DN(certificate.userID(0).id()).prettyDN(),
                      Formatting::complianceStringShort(certificate),
                      Formatting::creationDateString(certificate)));
            widgets.certificateInfo->setToolTip(Formatting::toolTip(certificate, toolTipOptions()));
            widgets.writeCertificateButton->setEnabled(true);
        } else {
            widgets.certificateInfo->setText(i18nc("@info", "<em>no matching certificate</em>"));
            widgets.certificateInfo->setToolTip(QString());
            widgets.writeCertificateButton->setEnabled(false);
        }
        if (card) {
            // update information if called with card
            widgets.keyGrip->setText(QString::fromStdString(grip));
            const std::string algo = card->keyAlgorithm(keyRef);
            widgets.keyAlgorithm->setText(algo.empty() ? i18nc("@info unknown key algorithm", "unknown") : QString::fromStdString(algo));
            widgets.importCertificateButton->setEnabled(!card->certificateData(keyRef).empty());
        }
        widgets.generateButton->setText(i18nc("@action:button", "Replace"));
        widgets.generateButton->setToolTip(
            i18nc("@info:tooltip %1 display name of a key", "Replace %1 with new key", PIVCard::keyDisplayName(keyRef)));
    }

    widgets.generateButton->setEnabled(true);
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

void PIVCardWidget::importCertificateFromCard(const std::string &keyref)
{
    auto cmd = new ImportCertificateFromPIVCardCommand(keyref, mCardSerialNumber);
    this->setEnabled(false);
    connect(cmd, &ImportCertificateFromPIVCardCommand::finished,
            this, [this, keyref] () {
                this->updateKeyWidgets(keyref, nullptr);
                this->setEnabled(true);
            });
    cmd->setParentWidget(this);
    cmd->start();
}

void PIVCardWidget::writeKeyToCard(const std::string &keyref)
{
    auto cmd = new KeyToCardCommand(keyref, mCardSerialNumber, PIVCard::AppName);
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
    auto cmd = new ChangePinCommand(mCardSerialNumber, PIVCard::AppName, this);
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
