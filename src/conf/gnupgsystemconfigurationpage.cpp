/* -*- mode: c++; c-basic-offset:4 -*-
    conf/gnupgsystemconfigurationpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>
#include "gnupgsystemconfigurationpage.h"

#include <Libkleo/CryptoConfigModule>
#include <QGpgME/Protocol>
#include <QGpgME/CryptoConfig>

#include <QVBoxLayout>

using namespace Kleo::Config;

GnuPGSystemConfigurationPage::GnuPGSystemConfigurationPage(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    auto lay = new QVBoxLayout(widget());
    lay->setContentsMargins(0, 0, 0, 0);

    QGpgME::CryptoConfig *const config = QGpgME::cryptoConfig();

    mWidget = new CryptoConfigModule(config, CryptoConfigModule::TabbedLayout, widget());
    lay->addWidget(mWidget);


    connect(mWidget, &CryptoConfigModule::changed, this, &Kleo::Config::GnuPGSystemConfigurationPage::markAsChanged);
}

GnuPGSystemConfigurationPage::~GnuPGSystemConfigurationPage()
{
    // ### correct here?
    if (QGpgME::CryptoConfig *const config = QGpgME::cryptoConfig()) {
        config->clear();
    }
}

void GnuPGSystemConfigurationPage::load()
{
    mWidget->reset();
}

void GnuPGSystemConfigurationPage::save()
{
    mWidget->save();
#if 0
    // Tell other apps (e.g. kmail) that the gpgconf data might have changed
    QDBusMessage message =
        QDBusMessage::createSignal(QString(), "org.kde.kleo.CryptoConfig", "changed");
    QDBusConnection::sessionBus().send(message);
#endif
}

void GnuPGSystemConfigurationPage::defaults()
{
    mWidget->defaults();
}

#include "moc_gnupgsystemconfigurationpage.cpp"
