/*  view/pivcardwiget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pivcardwidget.h"

#include "commands/pivgeneratecardkeycommand.h"

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

static QString keyDisplayName(std::string keyRef)
{
    static const QMap<std::string, QString> displayNames = {
        { PIVCard::pivAuthenticationKeyRef(), i18n("PIV Authentication Key") },
        { PIVCard::cardAuthenticationKeyRef(), i18n("Card Authentication Key") },
        { PIVCard::digitalSignatureKeyRef(), i18n("Digital Signature Key") },
        { PIVCard::keyManagementKeyRef(), i18n("Key Management Key") },
    };

    return displayNames.value(keyRef);
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
    mGenerateCardAuthenticationKeyBtn(new QPushButton(this)),
    mGenerateDigitalSignatureKeyBtn(new QPushButton(this)),
    mGenerateKeyManagementKeyBtn(new QPushButton(this))
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
    grid->addWidget(line1, row++, 0, 1, 4);
    grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(i18n("Keys:"))), row++, 0);

    grid->addWidget(new QLabel(i18n("PIV authentication:")), row, 0);
    grid->addWidget(mPIVAuthenticationKey, row, 1);
    mPIVAuthenticationKey->setTextInteractionFlags(Qt::TextBrowserInteraction);
    mGeneratePIVAuthenticationKeyBtn->setText(i18n("Generate"));
    mGeneratePIVAuthenticationKeyBtn->setEnabled(false);
    grid->addWidget(mGeneratePIVAuthenticationKeyBtn, row, 2);
    connect(mGeneratePIVAuthenticationKeyBtn, &QPushButton::clicked, this, &PIVCardWidget::generatePIVAuthenticationKey);
    row++;

    grid->addWidget(new QLabel(i18n("Card authentication:")), row, 0);
    grid->addWidget(mCardAuthenticationKey, row, 1);
    mCardAuthenticationKey->setTextInteractionFlags(Qt::TextBrowserInteraction);
    mGenerateCardAuthenticationKeyBtn->setText(i18n("Generate"));
    mGeneratePIVAuthenticationKeyBtn->setEnabled(false);
    grid->addWidget(mGenerateCardAuthenticationKeyBtn, row, 2);
    connect(mGenerateCardAuthenticationKeyBtn, &QPushButton::clicked, this, &PIVCardWidget::generateCardAuthenticationKey);
    row++;

    grid->addWidget(new QLabel(i18n("Digital signature:")), row, 0);
    grid->addWidget(mDigitalSignatureKey, row, 1);
    mDigitalSignatureKey->setTextInteractionFlags(Qt::TextBrowserInteraction);
    mGenerateDigitalSignatureKeyBtn->setText(i18n("Generate"));
    mGeneratePIVAuthenticationKeyBtn->setEnabled(false);
    grid->addWidget(mGenerateDigitalSignatureKeyBtn, row, 2);
    connect(mGenerateDigitalSignatureKeyBtn, &QPushButton::clicked, this, &PIVCardWidget::generateDigitalSignatureKey);
    row++;

    grid->addWidget(new QLabel(i18n("Key management:")), row, 0);
    grid->addWidget(mKeyManagementKey, row, 1);
    mKeyManagementKey->setTextInteractionFlags(Qt::TextBrowserInteraction);
    mGenerateKeyManagementKeyBtn->setText(i18n("Generate"));
    mGeneratePIVAuthenticationKeyBtn->setEnabled(false);
    grid->addWidget(mGenerateKeyManagementKeyBtn, row, 2);
    connect(mGenerateKeyManagementKeyBtn, &QPushButton::clicked, this, &PIVCardWidget::generateKeyManagementKey);
    row++;

    auto line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    grid->addWidget(line2, row++, 0, 1, 4);

    grid->setColumnStretch(4, -1);
}

PIVCardWidget::~PIVCardWidget()
{
}

void PIVCardWidget::setCard(const PIVCard *card)
{
    mCardSerialNumber = card->serialNumber();
    mVersionLabel->setText(i18nc("Placeholder is a version number", "PIV v%1 card", formatVersion(card->appVersion())));

    if (card->displaySerialNumber() != card->serialNumber()) {
        mSerialNumber->setText(QStringLiteral("%1 (%2)").arg(QString::fromStdString(card->displaySerialNumber()),
                                                             QString::fromStdString(card->serialNumber())));
    } else {
        mSerialNumber->setText(QString::fromStdString(card->serialNumber()));
    }

    updateKey(PIVCard::pivAuthenticationKeyRef(), card, mPIVAuthenticationKey, mGeneratePIVAuthenticationKeyBtn);
    updateKey(PIVCard::cardAuthenticationKeyRef(), card, mCardAuthenticationKey, mGenerateCardAuthenticationKeyBtn);
    updateKey(PIVCard::digitalSignatureKeyRef(), card, mDigitalSignatureKey, mGenerateDigitalSignatureKeyBtn);
    updateKey(PIVCard::keyManagementKeyRef(), card, mKeyManagementKey, mGenerateKeyManagementKeyBtn);
}

void PIVCardWidget::updateKey(const std::string &keyRef, const PIVCard *card, QLabel *label, QPushButton *button)
{
    const std::string grip = card->keyGrip(keyRef);
    label->setText(grip.empty() ? i18n("Slot empty") : QString::fromStdString(grip));
    button->setText(grip.empty() ? i18n("Generate") : i18n("Replace"));
    button->setToolTip(grip.empty() ?
        i18nc("Placeholder is the display name of a key", "Generate %1", keyDisplayName(keyRef)) :
        i18nc("Placeholder is the display name of a key", "Replace %1 with new key", keyDisplayName(keyRef)));
    button->setEnabled(true);
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

void PIVCardWidget::generatePIVAuthenticationKey()
{
    generateKey(PIVCard::pivAuthenticationKeyRef());
}

void PIVCardWidget::generateCardAuthenticationKey()
{
    generateKey(PIVCard::cardAuthenticationKeyRef());
}

void PIVCardWidget::generateDigitalSignatureKey()
{
    generateKey(PIVCard::digitalSignatureKeyRef());
}

void PIVCardWidget::generateKeyManagementKey()
{
    generateKey(PIVCard::keyManagementKeyRef());
}
