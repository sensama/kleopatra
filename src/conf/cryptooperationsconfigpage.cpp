/* -*- mode: c++; c-basic-offset:4 -*-
    conf/cryptooperationsconfigpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "cryptooperationsconfigpage.h"

#include "cryptooperationsconfigwidget.h"

#include <QVBoxLayout>

using namespace Kleo;
using namespace Kleo::Config;

#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
CryptoOperationsConfigurationPage::CryptoOperationsConfigurationPage(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
#else
CryptoOperationsConfigurationPage::CryptoOperationsConfigurationPage(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KCModule(parent, data, args)
#endif
{
#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
    auto lay = new QVBoxLayout(this);
    mWidget = new CryptoOperationsConfigWidget(this);
#else
    auto lay = new QVBoxLayout(widget());
    mWidget = new CryptoOperationsConfigWidget(widget());
#endif
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(mWidget);
    connect(mWidget, &CryptoOperationsConfigWidget::changed, this, &Kleo::Config::CryptoOperationsConfigurationPage::markAsChanged);
}

void CryptoOperationsConfigurationPage::load()
{
    mWidget->load();
}

void CryptoOperationsConfigurationPage::save()
{
    mWidget->save();
}

void CryptoOperationsConfigurationPage::defaults()
{
    mWidget->defaults();
}
