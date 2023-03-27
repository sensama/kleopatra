/* -*- mode: c++; c-basic-offset:4 -*-
    conf/smartcardconfigpage.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include <kcmutils_version.h>
#include <KCModule>

#include <memory>

namespace Kleo
{
namespace Config
{

class SmartCardConfigurationPage : public KCModule
{
    Q_OBJECT
public:
#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
    explicit SmartCardConfigurationPage(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
#else
    explicit SmartCardConfigurationPage(QObject *parent, const KPluginMetaData &data = {}, const QVariantList &args = QVariantList());
#endif
    ~SmartCardConfigurationPage() override;

    void load() override;
    void save() override;
    void defaults() override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}
