/*  view/openpgpkeycardwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "openpgpkeycardwidget.h"

#include "smartcard/card.h"
#include "smartcard/keypairinfo.h"
#include "smartcard/openpgpcard.h"

#include <Libkleo/Formatting>
#include <Libkleo/KeyCache>

#include <KLocalizedString>

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

#include <gpgme++/key.h>

using namespace Kleo;
using namespace SmartCard;

namespace
{
struct KeyWidgets {
    std::string keyGrip;
    std::string keyFingerprint;
    QLabel *keyInfoLabel = nullptr;
    QPushButton *createCSRButton = nullptr;
};

KeyWidgets createKeyWidgets(const KeyPairInfo &keyInfo, QWidget *parent)
{
    KeyWidgets keyWidgets;
    keyWidgets.keyInfoLabel = new QLabel{parent};
    keyWidgets.keyInfoLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    if (keyInfo.canCertify() || keyInfo.canSign() || keyInfo.canAuthenticate())
    {
        keyWidgets.createCSRButton = new QPushButton{i18nc("@action:button", "Create CSR"), parent};
        keyWidgets.createCSRButton->setToolTip(i18nc("@info:tooltip", "Create a certificate signing request for this key"));
        keyWidgets.createCSRButton->setEnabled(false);
    }

    return keyWidgets;
}
}

class OpenPGPKeyCardWidget::Private
{
public:
    explicit Private(OpenPGPKeyCardWidget *q);
    ~Private() = default;

    void update(const Card *card);

private:
    void updateKeyWidgets(const std::string &keyRef, const Card *card);

private:
    OpenPGPKeyCardWidget *const q;
    std::map<std::string, KeyWidgets> mKeyWidgets;
};

OpenPGPKeyCardWidget::Private::Private(OpenPGPKeyCardWidget *q)
    : q{q}
{
    auto grid = new QGridLayout{q};
    grid->setContentsMargins(0, 0, 0, 0);
    for (const auto &keyInfo : OpenPGPCard::supportedKeys()) {
        const KeyWidgets keyWidgets = createKeyWidgets(keyInfo, q);
        if (keyWidgets.createCSRButton) {
            const std::string keyRef = keyInfo.keyRef;
            connect(keyWidgets.createCSRButton, &QPushButton::clicked,
                    q, [q, keyRef] () { Q_EMIT q->createCSRRequested(keyRef); });
        }

        const int row = grid->rowCount();
        grid->addWidget(new QLabel{OpenPGPCard::keyDisplayName(keyInfo.keyRef)}, row, 0);
        grid->addWidget(keyWidgets.keyInfoLabel, row, 1);
        if (keyWidgets.createCSRButton) {
            grid->addWidget(keyWidgets.createCSRButton, row, 2);
        }

        mKeyWidgets.insert({keyInfo.keyRef, keyWidgets});
    }
    grid->setColumnStretch(grid->columnCount(), 1);
}

void OpenPGPKeyCardWidget::Private::update(const Card *card)
{
    updateKeyWidgets(OpenPGPCard::pgpSigKeyRef(), card);
    updateKeyWidgets(OpenPGPCard::pgpEncKeyRef(), card);
    updateKeyWidgets(OpenPGPCard::pgpAuthKeyRef(), card);
}

void OpenPGPKeyCardWidget::Private::updateKeyWidgets(const std::string &keyRef, const Card *card)
{
    KeyWidgets widgets = mKeyWidgets.at(keyRef);
    const std::string grip = card ? card->keyInfo(keyRef).grip : widgets.keyGrip;
    widgets.keyGrip = grip;
    if (grip.empty()) {
        widgets.keyInfoLabel->setText(i18n("Slot empty"));
        if (widgets.createCSRButton) {
            widgets.createCSRButton->setEnabled(false);
        }
    } else {
        const std::string fpr = card ? card->keyFingerprint(keyRef) : widgets.keyFingerprint;
        widgets.keyFingerprint = fpr;
        widgets.keyInfoLabel->setText(QString::fromStdString(fpr));
        if (fpr.size() >= 16) {
            const std::string keyid = fpr.substr(fpr.size() - 16);
            const auto subkeys = KeyCache::instance()->findSubkeysByKeyID({keyid});
            if (subkeys.empty() || subkeys[0].isNull()) {
                widgets.keyInfoLabel->setToolTip(i18n("Public key not found."));
            } else {
                QStringList toolTips;
                toolTips.reserve(subkeys.size());
                for (const auto &sub: subkeys) {
                    // Yep you can have one subkey associated with multiple primary keys.
                    toolTips << Formatting::toolTip(sub.parent(), Formatting::Validity |
                                                    Formatting::StorageLocation |
                                                    Formatting::ExpiryDates |
                                                    Formatting::UserIDs |
                                                    Formatting::Fingerprint);
                }
                widgets.keyInfoLabel->setToolTip(toolTips.join(QLatin1String("<br/>")));
            }
        }
        if (widgets.createCSRButton) {
            widgets.createCSRButton->setEnabled(true);
        }
    }
}

OpenPGPKeyCardWidget::OpenPGPKeyCardWidget(QWidget *parent)
    : QWidget{parent}
    , d{std::make_unique<Private>(this)}
{
}

OpenPGPKeyCardWidget::~OpenPGPKeyCardWidget() = default;

void OpenPGPKeyCardWidget::update(const Card *card)
{
    d->update(card);
}
