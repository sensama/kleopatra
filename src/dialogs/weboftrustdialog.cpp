/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/weboftrustdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "weboftrustdialog.h"

#include "weboftrustwidget.h"

#include "commands/importcertificatefromkeyservercommand.h"

#include <Libkleo/KeyCache>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include <KSharedConfig>
#include <KConfigGroup>

#include <algorithm>
#include <set>

using namespace Kleo;

WebOfTrustDialog::WebOfTrustDialog(QWidget *parent)
    : QDialog(parent)
{
    KConfigGroup dialog(KSharedConfig::openStateConfig(), "WebOfTrustDialog");
    const QSize size = dialog.readEntry("Size", QSize(900, 400));
    if (size.isValid()) {
        resize(size);
    }
    setWindowTitle(i18nc("@title:window", "Certifications"));

    mWidget = new WebOfTrustWidget(this);
    auto l = new QVBoxLayout(this);
    l->addWidget(mWidget);

    auto bbox = new QDialogButtonBox(this);

    auto btn = bbox->addButton(QDialogButtonBox::Close);
    connect(btn, &QPushButton::pressed, this, &QDialog::accept);

    mFetchKeysBtn = bbox->addButton(i18nc("@action:button", "Fetch Missing Keys"),
                                    QDialogButtonBox::ActionRole);
    mFetchKeysBtn->setToolTip(i18nc("@info:tooltip", "Look up and import all keys that were used to certify the user ids of this key"));
    connect(mFetchKeysBtn, &QPushButton::pressed, this, &WebOfTrustDialog::fetchMissingKeys);
#ifndef QGPGME_SUPPORTS_RECEIVING_KEYS_BY_KEY_ID
    mFetchKeysBtn->setVisible(false);
#endif

    l->addWidget(bbox);
}

void WebOfTrustDialog::setKey(const GpgME::Key &key)
{
    mWidget->setKey(key);
    mFetchKeysBtn->setEnabled(!key.isBad());
}

GpgME::Key WebOfTrustDialog::key() const
{
    return mWidget->key();
}

WebOfTrustDialog::~WebOfTrustDialog()
{
    KConfigGroup dialog(KSharedConfig::openStateConfig(), "WebOfTrustDialog");
    dialog.writeEntry("Size", size());
    dialog.sync();
}

namespace
{
bool havePublicKeyForSignature(const GpgME::UserID::Signature &signature)
{
    // GnuPG returns status "NoPublicKey" for missing signing keys, but also
    // for expired or revoked signing keys.
    return (signature.status() != GpgME::UserID::Signature::NoPublicKey)
        || !KeyCache::instance()->findByKeyIDOrFingerprint (signature.signerKeyID()).isNull();
}

auto accumulateMissingSignerKeys(const std::vector<GpgME::UserID::Signature> &signatures)
{
    return std::accumulate(
        std::begin(signatures), std::end(signatures),
        std::set<QString>{},
        [] (auto &keyIds, const auto &signature) {
            if (!havePublicKeyForSignature(signature)) {
                keyIds.insert(QLatin1String{signature.signerKeyID()});
            }
            return keyIds;
        }
    );
}

auto accumulateMissingSignerKeys(const std::vector<GpgME::UserID> &userIds)
{
    return std::accumulate(
        std::begin(userIds), std::end(userIds),
        std::set<QString>(),
        [] (auto &keyIds, const auto &userID) {
            if (!userID.isBad()) {
                const auto newKeyIds = accumulateMissingSignerKeys(userID.signatures());
                std::copy(std::begin(newKeyIds), std::end(newKeyIds),
                          std::inserter(keyIds, std::end(keyIds)));
            }
            return keyIds;
        }
    );
}
}

void WebOfTrustDialog::fetchMissingKeys()
{
    if (key().isNull()) {
        return;
    }
    const auto missingSignerKeys = accumulateMissingSignerKeys(key().userIDs());

    auto cmd = new Kleo::ImportCertificateFromKeyserverCommand{QStringList{std::begin(missingSignerKeys), std::end(missingSignerKeys)}};
    cmd->setParentWidget(this);
    setEnabled(false);
    connect(cmd, &Kleo::ImportCertificateFromKeyserverCommand::finished,
            this, [this]() {
        // Trigger an update when done
        setKey(key());
        setEnabled(true);
    });
    cmd->start();
}
