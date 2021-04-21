/*  view/p15cardwiget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Andre Heinecke <aheinecke@g10code.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "p15cardwidget.h"

#include "smartcard/p15card.h"
#include "smartcard/openpgpcard.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QStringList>

#include <KLocalizedString>
#include <KSeparator>

#include <Libkleo/KeyCache>
#include <Libkleo/Formatting>

using namespace Kleo;
using namespace Kleo::SmartCard;

P15CardWidget::P15CardWidget(QWidget *parent)
    : QWidget(parent)
    , mSerialNumber(new QLabel(this))
    , mVersionLabel(new QLabel(this))
    , mSigFprLabel(new QLabel(this))
    , mEncFprLabel(new QLabel(this))
{
    // Set up the scroll area
    auto myLayout = new QVBoxLayout(this);
    myLayout->setContentsMargins(0, 0, 0, 0);

    auto area = new QScrollArea;
    area->setFrameShape(QFrame::NoFrame);
    area->setWidgetResizable(true);
    myLayout->addWidget(area);

    auto areaWidget = new QWidget;
    area->setWidget(areaWidget);

    auto areaVLay = new QVBoxLayout(areaWidget);

    auto cardInfoGrid = new QGridLayout;
    {
        int row = 0;

        // Version and Serialnumber
        cardInfoGrid->addWidget(mVersionLabel, row++, 0, 1, 2);
        mVersionLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

        cardInfoGrid->addWidget(new QLabel(i18n("Serial number:")), row, 0);
        cardInfoGrid->addWidget(mSerialNumber, row++, 1);
        mSerialNumber->setTextInteractionFlags(Qt::TextBrowserInteraction);

        cardInfoGrid->setColumnStretch(cardInfoGrid->columnCount(), 1);
    }
    areaVLay->addLayout(cardInfoGrid);

    areaVLay->addWidget(new KSeparator(Qt::Horizontal));

    areaVLay->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(i18n("OpenPGP keys:"))));
    areaVLay->addWidget(mSigFprLabel);
    areaVLay->addWidget(mEncFprLabel);

    areaVLay->addWidget(new KSeparator(Qt::Horizontal));
    areaVLay->addStretch(1);
}

P15CardWidget::~P15CardWidget()
{
}

void P15CardWidget::setCard(const P15Card *card)
{
    mCardSerialNumber = card->serialNumber();
    mVersionLabel->setText(i18nc("%1 is a smartcard manufacturer", "%1 PKCS#15 card",
                           QString::fromStdString(card->manufacturer())));
    mSerialNumber->setText(card->displaySerialNumber());
    mSerialNumber->setToolTip(QString::fromStdString(card->serialNumber()));

    std::string keyid = card->appKeyFingerprint(OpenPGPCard::pgpSigKeyRef());
    if (!keyid.empty()) {
        QString text = i18n("Signing key:") +
            QStringLiteral("\t%1 (%2)")
            .arg(QString::fromStdString(keyid))
            .arg(QString::fromStdString(card->signingKeyRef()));
        text += QStringLiteral("<br/><br/>");

        keyid.erase(0, keyid.size() - 16);
        const auto subkeys = KeyCache::instance()->findSubkeysByKeyID({keyid});
        if (subkeys.empty() || subkeys[0].isNull()) {
            text += i18n("Public key not found.");
        } else {

            QStringList toolTips;
            toolTips.reserve(subkeys.size());
            for (const auto &sub: subkeys) {
                // Yep you can have one subkey associated with multiple
                // primary keys.
                toolTips << Formatting::toolTip(sub.parent(), Formatting::Validity |
                        Formatting::ExpiryDates |
                        Formatting::UserIDs |
                        Formatting::Fingerprint);
            }
            text += toolTips.join(QLatin1String("<br/>"));
        }
        mSigFprLabel->setText(text);
    } else {
        mSigFprLabel->setVisible(false);
    }
    keyid = card->appKeyFingerprint(OpenPGPCard::pgpEncKeyRef());
    if (!keyid.empty()) {
        mEncFprLabel->setText(i18n("Encryption key:") +
                QStringLiteral(" %1 (%2)").arg(QString::fromStdString(keyid))
                .arg(QString::fromStdString(card->encryptionKeyRef())));
        keyid.erase(0, keyid.size() - 16);
        const auto subkeys = KeyCache::instance()->findSubkeysByKeyID({keyid});
        if (subkeys.empty() || subkeys[0].isNull()) {
            mEncFprLabel->setToolTip(i18n("Public key not found."));
        } else {
            QStringList toolTips;
            toolTips.reserve(subkeys.size());
            for (const auto &sub: subkeys) {
                // Yep you can have one subkey associated with multiple
                // primary keys.
                toolTips << Formatting::toolTip(sub.parent(), Formatting::Validity |
                        Formatting::StorageLocation |
                        Formatting::ExpiryDates |
                        Formatting::UserIDs |
                        Formatting::Fingerprint);

            }
            mEncFprLabel->setToolTip(toolTips.join(QLatin1String("<br/>")));
        }
    } else {
        mEncFprLabel->setVisible(false);
    }

//    updateKeyWidgets(OpenPGPCard::pgpSigKeyRef(), card);
//    updateKeyWidgets(OpenPGPCard::pgpEncKeyRef(), card);
}
