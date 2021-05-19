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
    QLabel *keyTitleLabel = nullptr;
    QLabel *keyInfoLabel = nullptr;
    QPushButton *createCSRButton = nullptr;
};

KeyWidgets createKeyWidgets(const KeyPairInfo &keyInfo, QWidget *parent)
{
    KeyWidgets keyWidgets;
    keyWidgets.keyTitleLabel = new QLabel{OpenPGPCard::keyDisplayName(keyInfo.keyRef), parent};
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
    void updateCachedValues(const std::string &keyRef, const Card *card);
    void updateKeyWidgets(const std::string &keyRef);

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
        grid->addWidget(keyWidgets.keyTitleLabel, row, 0, Qt::AlignTop);
        grid->addWidget(keyWidgets.keyInfoLabel, row, 1);
        if (keyWidgets.createCSRButton) {
            grid->addWidget(keyWidgets.createCSRButton, row, 2, Qt::AlignTop);
        }

        mKeyWidgets.insert({keyInfo.keyRef, keyWidgets});
    }
    grid->setColumnStretch(grid->columnCount(), 1);
}

void OpenPGPKeyCardWidget::Private::update(const Card *card)
{
    if (card) {
        updateCachedValues(OpenPGPCard::pgpSigKeyRef(), card);
        updateCachedValues(OpenPGPCard::pgpEncKeyRef(), card);
        updateCachedValues(OpenPGPCard::pgpAuthKeyRef(), card);
    }
    updateKeyWidgets(OpenPGPCard::pgpSigKeyRef());
    updateKeyWidgets(OpenPGPCard::pgpEncKeyRef());
    updateKeyWidgets(OpenPGPCard::pgpAuthKeyRef());
}

void OpenPGPKeyCardWidget::Private::updateCachedValues(const std::string &keyRef, const Card *card)
{
    KeyWidgets &widgets = mKeyWidgets.at(keyRef);
    widgets.keyGrip = card->keyInfo(keyRef).grip;
    widgets.keyFingerprint = card->keyFingerprint(keyRef);
}

void OpenPGPKeyCardWidget::Private::updateKeyWidgets(const std::string &keyRef)
{
    const KeyWidgets &widgets = mKeyWidgets.at(keyRef);

    if (widgets.keyGrip.empty()) {
        widgets.keyInfoLabel->setText(i18n("Slot empty"));
        if (widgets.createCSRButton) {
            widgets.createCSRButton->setEnabled(false);
        }
    } else {
        QStringList lines = {Formatting::prettyID(widgets.keyFingerprint.c_str())};
        if (widgets.keyFingerprint.size() >= 16) {
            const std::string keyid = widgets.keyFingerprint.substr(widgets.keyFingerprint.size() - 16);
            const auto subkeys = KeyCache::instance()->findSubkeysByKeyID({keyid});
            if (subkeys.empty() || subkeys[0].isNull()) {
                lines.push_back(i18n("Public key not found locally"));
                widgets.keyInfoLabel->setToolTip({});
            } else {
                QStringList toolTips;
                toolTips.reserve(subkeys.size());
                for (const auto &sub: subkeys) {
                    // Yep you can have one subkey associated with multiple primary keys.
                    const GpgME::Key key = sub.parent();
                    toolTips << Formatting::toolTip(key,
                                                    Formatting::Validity |
                                                    Formatting::ExpiryDates |
                                                    Formatting::UserIDs |
                                                    Formatting::Fingerprint);
                    const auto uids = key.userIDs();
                    for (const auto &uid: uids) {
                        lines.push_back(Formatting::prettyUserID(uid));
                    }
                }
                widgets.keyInfoLabel->setToolTip(toolTips.join(QLatin1String("<br/>")));
            }
        }

        widgets.keyInfoLabel->setText(lines.join(QLatin1Char('\n')));

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
