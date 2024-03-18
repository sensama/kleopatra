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

AppearanceConfigurationPage::AppearanceConfigurationPage(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    auto lay = new QVBoxLayout(widget());
    lay->setContentsMargins({});
    mWidget = new AppearanceConfigWidget(widget());
    lay->addWidget(mWidget);

    connect(mWidget, &AppearanceConfigWidget::changed, this, &Kleo::Config::AppearanceConfigurationPage::markAsChanged);

    load();
    setNeedsSave(false);
}

void AppearanceConfigurationPage::load()
{
    mWidget->load();
}

void AppearanceConfigurationPage::save()
{
    mWidget->save();
    setNeedsSave(false);
}

void AppearanceConfigurationPage::defaults()
{
    mWidget->defaults();
}

#include "moc_appearanceconfigpage.cpp"
