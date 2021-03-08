/* -*- mode: c++; c-basic-offset:4 -*-
    conf/dirservconfigpage.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2004, 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DIRSERVCONFIGPAGE_H
#define DIRSERVCONFIGPAGE_H

#include <KCModule>

#include <QGpgME/CryptoConfig>

class QCheckBox;
class QLabel;
class QTimeEdit;
class QSpinBox;
namespace Kleo
{
class DirectoryServicesWidget;
}

/**
 * "Directory Services" configuration page for kleopatra's configuration dialog
 * The user can configure LDAP servers in this page, to be used for listing/fetching
 * remote certificates in kleopatra.
 */
class DirectoryServicesConfigurationPage : public KCModule
{
    Q_OBJECT
public:
    explicit DirectoryServicesConfigurationPage(QWidget *parent = nullptr, const QVariantList &args = QVariantList());

    void load() override;
    void save() override;
    void defaults() override;

private:
    enum EntryMultiplicity {
        SingleValue,
        ListValue
    };
    enum ShowError {
        DoNotShowError,
        DoShowError
    };

    QGpgME::CryptoConfigEntry *configEntry(const char *componentName,
                                           const char *entryName,
                                           QGpgME::CryptoConfigEntry::ArgType argType,
                                           EntryMultiplicity multiplicity,
                                           ShowError showError);

    Kleo::DirectoryServicesWidget *mWidget;
    QTimeEdit *mTimeout;
    QSpinBox *mMaxItems;
    QLabel *mMaxItemsLabel;
    QCheckBox *mAddNewServersCB;

    QGpgME::CryptoConfigEntry *mX509ServicesEntry;
    QGpgME::CryptoConfigEntry *mOpenPGPServiceEntry;
    QGpgME::CryptoConfigEntry *mTimeoutConfigEntry;
    QGpgME::CryptoConfigEntry *mMaxItemsConfigEntry;
    QGpgME::CryptoConfigEntry *mAddNewServersConfigEntry;

    QGpgME::CryptoConfig *mConfig;
};

#endif
