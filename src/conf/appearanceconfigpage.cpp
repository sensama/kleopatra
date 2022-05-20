/* -*- mode: c++; c-basic-offset:4 -*-
    conf/appearanceconfigpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2004, 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "appearanceconfigpage.h"

#include "appearanceconfigwidget.h"


#include <QVBoxLayout>

using namespace Kleo;
using namespace Kleo::Config;

#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
AppearanceConfigurationPage::AppearanceConfigurationPage(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
#else
AppearanceConfigurationPage::AppearanceConfigurationPage(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KCModule(parent, data, args)
#endif
{
#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
    auto lay = new QVBoxLayout(this);
    mWidget = new AppearanceConfigWidget(this);
#else
    auto lay = new QVBoxLayout(widget());
    mWidget = new AppearanceConfigWidget(widget());
#endif
    lay->addWidget(mWidget);

    connect(mWidget, &AppearanceConfigWidget::changed, this, &Kleo::Config::AppearanceConfigurationPage::markAsChanged);
}

void AppearanceConfigurationPage::load()
{
    mWidget->load();
}

void AppearanceConfigurationPage::save()
{
    mWidget->save();

}

void AppearanceConfigurationPage::defaults()
{
    mWidget->defaults();
}
