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

CryptoOperationsConfigurationPage::CryptoOperationsConfigurationPage(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    auto lay = new QVBoxLayout(widget());
    mWidget = new CryptoOperationsConfigWidget(widget());
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
