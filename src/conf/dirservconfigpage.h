/* -*- mode: c++; c-basic-offset:4 -*-
    conf/dirservconfigpage.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2004, 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include <kcmutils_version.h>
#include <KCModule>

#include <memory>

/**
 * "Directory Services" configuration page for kleopatra's configuration dialog
 * The user can configure LDAP servers in this page, to be used for listing/fetching
 * remote certificates in kleopatra.
 */
class DirectoryServicesConfigurationPage : public KCModule
{
    Q_OBJECT
public:
#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
    explicit DirectoryServicesConfigurationPage(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
#else
    explicit DirectoryServicesConfigurationPage(QObject *parent, const KPluginMetaData &data = {}, const QVariantList &args = QVariantList());
#endif
    ~DirectoryServicesConfigurationPage() override;

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

private:
    class Private;
    const std::unique_ptr<Private> d;
    friend class DirectoryServicesConfigurationPage::Private;
};
