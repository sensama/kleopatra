/* -*- mode: c++; c-basic-offset:4 -*-
    conf/smimevalidationconfigurationpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "smimevalidationconfigurationpage.h"
#include "smimevalidationconfigurationwidget.h"

#include <QVBoxLayout>

using namespace Kleo::Config;

#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
SMimeValidationConfigurationPage::SMimeValidationConfigurationPage(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
#else
SMimeValidationConfigurationPage::SMimeValidationConfigurationPage(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KCModule(parent, data, args)
#endif
{
#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
    auto lay = new QVBoxLayout(this);
#else
    auto lay = new QVBoxLayout(widget());
#endif
    lay->setContentsMargins(0, 0, 0, 0);

#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
    mWidget = new SMimeValidationConfigurationWidget(this);
#else
    mWidget = new SMimeValidationConfigurationWidget(widget());
#endif
    lay->addWidget(mWidget);

    connect(mWidget, &SMimeValidationConfigurationWidget::changed, this, &Kleo::Config::SMimeValidationConfigurationPage::markAsChanged);
}

void SMimeValidationConfigurationPage::load()
{
    mWidget->load();
}

void SMimeValidationConfigurationPage::save()
{
    mWidget->save();
}

void SMimeValidationConfigurationPage::defaults()
{
    mWidget->defaults();
}
