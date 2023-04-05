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

SMimeValidationConfigurationPage::SMimeValidationConfigurationPage(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KCModule(parent, data, args)
{
    auto lay = new QVBoxLayout(widget());
    lay->setContentsMargins(0, 0, 0, 0);

    mWidget = new SMimeValidationConfigurationWidget(widget());
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
