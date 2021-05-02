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

AppearanceConfigurationPage::AppearanceConfigurationPage(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
{
    auto lay = new QVBoxLayout(this);
    mWidget = new AppearanceConfigWidget(this);
    lay->addWidget(mWidget);

    connect(mWidget, &AppearanceConfigWidget::changed, this, &Kleo::Config::AppearanceConfigurationPage::markAsChanged);

    load();
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

extern "C"
{
    Q_DECL_EXPORT KCModule *create_kleopatra_config_appear(QWidget *parent = nullptr, const QVariantList &args = QVariantList())
    {
        auto page =
            new AppearanceConfigurationPage(parent, args);
        page->setObjectName(QStringLiteral("kleopatra_config_appear"));
        return page;
    }
}

