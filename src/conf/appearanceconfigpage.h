/* -*- mode: c++; c-basic-offset:4 -*-
    conf/appearanceconfigpage.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2004, 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include <kcmutils_version.h>
#include <KCModule>
namespace Kleo
{
namespace Config
{

class AppearanceConfigWidget;

/**
 * "Appearance" configuration page for kleopatra's configuration dialog
 */
class AppearanceConfigurationPage : public KCModule
{
    Q_OBJECT
public:
#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
    explicit AppearanceConfigurationPage(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
#else
    explicit AppearanceConfigurationPage(QObject *parent, const KPluginMetaData &data = {}, const QVariantList &args = QVariantList());
#endif

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

private:
    AppearanceConfigWidget *mWidget;
};

}
}

