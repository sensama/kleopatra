/*  view/p15cardwiget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Andre Heinecke <aheinecke@g10code.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "p15cardwidget.h"

#include "smartcard/p15card.h"
#include "smartcard/openpgpcard.h"
#include "smartcard/readerstatus.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QStringList>

#include <KLocalizedString>
#include <KSeparator>

#include <Libkleo/KeyCache>
#include <Libkleo/Formatting>

#include <QGpgME/Protocol>
#include <QGpgME/KeyListJob>
#include <QGpgME/ImportFromKeyserverJob>
#include <QGpgME/CryptoConfig>
#include <gpgme++/keylistresult.h>
#include <gpgme++/importresult.h>

#include <Libkleo/GnuPG>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::SmartCard;

P15CardWidget::P15CardWidget(QWidget *parent)
    : QWidget(parent)
    , mSerialNumber(new QLabel(this))
    , mVersionLabel(new QLabel(this))
    , mSigFprLabel(new QLabel(this))
    , mEncFprLabel(new QLabel(this))
    , mStatusLabel(new QLabel(this))
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
    areaVLay->addWidget(mStatusLabel);
    mStatusLabel->setVisible(false);

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

void P15CardWidget::searchPGPFpr(const std::string &fpr)
{
    /* Only do auto import from LDAP */
    auto conf = QGpgME::cryptoConfig();
    Q_ASSERT (conf);
    const QString cmp = engineIsVersion(2, 3, 0) ? QStringLiteral("dirmngr") : QStringLiteral("gpg");
    const auto entry = conf->entry(cmp, QStringLiteral("Keyserver"), QStringLiteral("keyserver"));
    if (!entry || !entry->stringValue().startsWith(QStringLiteral("ldap"))) {
        return;
    }
    static std::vector<std::string> fprs;
    if (fpr.empty() || std::find(fprs.begin(), fprs.end(), fpr) != fprs.end()) {
        qCDebug(KLEOPATRA_LOG) << "Already looked for " << fpr.c_str() << "on ldap server";
        return;
    }
    mStatusLabel->setText(i18n("Searching in directory service..."));
    mStatusLabel->setVisible(true);
    fprs.push_back (fpr);
    qCDebug(KLEOPATRA_LOG) << "Looking for:" << fpr.c_str() << "on ldap server";
    QGpgME::KeyListJob *job = QGpgME::openpgp()->keyListJob(true);
    connect(KeyCache::instance().get(), &KeyCache::keysMayHaveChanged, this, [this, fpr] () {
            qCDebug(KLEOPATRA_LOG) << "Updating key info after changes";
            ReaderStatus::mutableInstance()->updateStatus();
            updateSigKeyWidgets(fpr);
    });
    connect(job, &QGpgME::KeyListJob::result, job, [this](GpgME::KeyListResult, std::vector<GpgME::Key> keys, QString, GpgME::Error) {
        if (keys.size() == 1) {
            auto importJob = QGpgME::openpgp()->importFromKeyserverJob();
            qCDebug(KLEOPATRA_LOG) << "Importing: " << keys[0].primaryFingerprint();
            connect(importJob, &QGpgME::ImportFromKeyserverJob::result, importJob, [this](GpgME::ImportResult, QString, GpgME::Error) {
                qCDebug(KLEOPATRA_LOG) << "import job done";
                mStatusLabel->setText(i18n("Automatic import finished."));
            });
            importJob->start(keys);
        } else if (keys.size() > 1) {
            qCDebug(KLEOPATRA_LOG) << "Multiple keys found on server";
            mStatusLabel->setText(i18n("Error multiple keys found on server."));
        } else {
            qCDebug(KLEOPATRA_LOG) << "No key found";
            mStatusLabel->setText(i18n("Key not found in directory service."));
        }
    });
    job->start(QStringList() << QString::fromStdString(fpr));

}

void P15CardWidget::updateSigKeyWidgets(const std::string &fpr)
{
    std::string keyid = fpr;
    if (!keyid.empty()) {
        QString text = i18n("Signing key:") +
            QStringLiteral("\t%1 (%2)")
            .arg(Formatting::prettyID(keyid.c_str()));
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
}

void P15CardWidget::setCard(const P15Card *card)
{
    mCardSerialNumber = card->serialNumber();
    mVersionLabel->setText(i18nc("%1 is a smartcard manufacturer", "%1 PKCS#15 card",
                           QString::fromStdString(card->manufacturer())));
    mSerialNumber->setText(card->displaySerialNumber());
    mSerialNumber->setToolTip(QString::fromStdString(card->serialNumber()));

    const auto sigInfo = card->keyInfo(card->signingKeyRef());
    if (!sigInfo.grip.empty()) {
        const auto key = KeyCache::instance()->findSubkeyByKeyGrip(sigInfo.grip, GpgME::OpenPGP).parent();
        if (key.isNull()) {
            qCDebug(KLEOPATRA_LOG) << "Failed to find key for grip:" << sigInfo.grip.c_str();
            const auto pgpSigFpr = card->keyFingerprint(OpenPGPCard::pgpSigKeyRef());
            if (!pgpSigFpr.empty()) {
                qCDebug(KLEOPATRA_LOG) << "Should be pgp key:" << pgpSigFpr.c_str();
                searchPGPFpr(pgpSigFpr);
            }
        } else {
            mStatusLabel->setVisible(false);
        }
    }

    std::string keyid = card->keyFingerprint(OpenPGPCard::pgpSigKeyRef());
    updateSigKeyWidgets(keyid);
    keyid = card->keyFingerprint(OpenPGPCard::pgpEncKeyRef());
    if (!keyid.empty()) {
        mEncFprLabel->setText(i18n("Encryption key:") +
                QStringLiteral(" %1 (%2)")
                .arg(Formatting::prettyID(keyid.c_str()))
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
