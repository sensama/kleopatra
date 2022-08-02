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

#include <Libkleo/KeyHelpers>

#include <QAction>
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

namespace
{
void addActionButton(QDialogButtonBox *buttonBox, QAction *action)
{
    if (!action) {
        return;
    }
    auto button = buttonBox->addButton(action->text(), QDialogButtonBox::ActionRole);
    button->setEnabled(action->isEnabled());
    QObject::connect(action, &QAction::changed, button, [action, button]() {
        button->setEnabled(action->isEnabled());
    });
    QObject::connect(button, &QPushButton::clicked, action, &QAction::trigger);
}
}

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

    addActionButton(bbox, mWidget->detailsAction());
    addActionButton(bbox, mWidget->certifyAction());
    addActionButton(bbox, mWidget->revokeAction());

    mFetchKeysBtn = bbox->addButton(i18nc("@action:button", "Fetch Missing Keys"),
                                    QDialogButtonBox::ActionRole);
    mFetchKeysBtn->setToolTip(i18nc("@info:tooltip", "Look up and import all keys that were used to certify the user IDs of this key"));
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

void WebOfTrustDialog::fetchMissingKeys()
{
    if (key().isNull()) {
        return;
    }
    const auto missingSignerKeyIds = Kleo::getMissingSignerKeyIds(key().userIDs());

    auto cmd = new Kleo::ImportCertificateFromKeyserverCommand{QStringList{std::begin(missingSignerKeyIds), std::end(missingSignerKeyIds)}};
    cmd->setParentWidget(this);
    mFetchKeysBtn->setEnabled(false);
    connect(cmd, &Kleo::ImportCertificateFromKeyserverCommand::finished,
            this, [this]() {
        // Trigger an update when done
        setKey(key());
        mFetchKeysBtn->setEnabled(true);
    });
    cmd->start();
}
