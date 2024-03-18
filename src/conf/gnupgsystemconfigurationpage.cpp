/* -*- mode: c++; c-basic-offset:4 -*-
    conf/gnupgsystemconfigurationpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gnupgsystemconfigurationpage.h"
#include <config-kleopatra.h>

#include <Libkleo/CryptoConfigModule>
#include <QGpgME/CryptoConfig>
#include <QGpgME/Protocol>

#include <QVBoxLayout>

using namespace Kleo::Config;

#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
GnuPGSystemConfigurationPage::GnuPGSystemConfigurationPage(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
#else
GnuPGSystemConfigurationPage::GnuPGSystemConfigurationPage(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KCModule(parent, data, args)
#endif
{
#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
    auto lay = new QVBoxLayout(this);
#else
    auto lay = new QVBoxLayout(widget());
#endif
    lay->setContentsMargins(0, 0, 0, 0);

    QGpgME::CryptoConfig *const config = QGpgME::cryptoConfig();

    mWidget = new CryptoConfigModule(config,
                                     CryptoConfigModule::TabbedLayout,
#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
                                     this);
#else
                                     widget());
#endif
    lay->addWidget(mWidget);

    connect(mWidget, &CryptoConfigModule::changed, this, &Kleo::Config::GnuPGSystemConfigurationPage::markAsChanged);

    load();
    setNeedsSave(false);
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
    setNeedsSave(false);
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
