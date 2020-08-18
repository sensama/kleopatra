/*  view/pivcardwiget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pivcardwidget.h"

#include "smartcard/pivcard.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include <KLocalizedString>

using namespace Kleo;
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
    mPivAuthenticationKey(new QLabel(this)),
    mCardAuthenticationKey(new QLabel(this)),
    mSigningKey(new QLabel(this)),
    mEncryptionKey(new QLabel(this)),
    mCardIsEmpty(false)
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
    grid->addWidget(mPivAuthenticationKey, row++, 1);
    mPivAuthenticationKey->setTextInteractionFlags(Qt::TextBrowserInteraction);

    grid->addWidget(new QLabel(i18n("Card authentication:")), row, 0);
    grid->addWidget(mCardAuthenticationKey, row++, 1);
    mCardAuthenticationKey->setTextInteractionFlags(Qt::TextBrowserInteraction);

    grid->addWidget(new QLabel(i18n("Digital signature:")), row, 0);
    grid->addWidget(mSigningKey, row++, 1);
    mSigningKey->setTextInteractionFlags(Qt::TextBrowserInteraction);

    grid->addWidget(new QLabel(i18n("Key management:")), row, 0);
    grid->addWidget(mEncryptionKey, row++, 1);
    mEncryptionKey->setTextInteractionFlags(Qt::TextBrowserInteraction);

    auto line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    grid->addWidget(line2, row++, 0, 1, 4);

    grid->setColumnStretch(4, -1);
}

void PIVCardWidget::setCard(const PIVCard *card)
{
    mVersionLabel->setText(i18nc("Placeholder is a version number", "PIV v%1 card", formatVersion(card->appVersion())));

    if (card->displaySerialNumber() != card->serialNumber()) {
        mSerialNumber->setText(QStringLiteral("%1 (%2)").arg(QString::fromStdString(card->displaySerialNumber()),
                                                             QString::fromStdString(card->serialNumber())));
    } else {
        mSerialNumber->setText(QString::fromStdString(card->serialNumber()));
    }

    updateKey(mPivAuthenticationKey, card->pivAuthenticationKeyGrip());
    updateKey(mCardAuthenticationKey, card->cardAuthenticationKeyGrip());
    updateKey(mSigningKey, card->digitalSignatureKeyGrip());
    updateKey(mEncryptionKey, card->keyManagementKeyGrip());
    mCardIsEmpty = card->pivAuthenticationKeyGrip().empty()
        && card->cardAuthenticationKeyGrip().empty()
        && card->digitalSignatureKeyGrip().empty()
        && card->keyManagementKeyGrip().empty();
}

void PIVCardWidget::updateKey(QLabel *label, const std::string &grip)
{
    label->setText(QString::fromStdString(grip));

    if (grip.empty()) {
        label->setText(i18n("Slot empty"));
        return;
    }
}
