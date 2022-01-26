/* -*- mode: c++; c-basic-offset:4 -*-
    conf/dirservconfigpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2004, 2008 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "dirservconfigpage.h"

#include "labelledwidget.h"

#include <settings.h>

#include <Libkleo/Compat>
#include <Libkleo/DirectoryServicesWidget>
#include <Libkleo/KeyserverConfig>

#include <QGpgME/CryptoConfig>
#include <QGpgME/Protocol>

#include <KMessageBox>
#include <KLocalizedString>
#include "kleopatra_debug.h"
#include <KConfig>
#include <QSpinBox>

#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QLayout>
#include <QLineEdit>
#include <QTimeEdit>
#include <QVBoxLayout>

#include <gpgme++/engineinfo.h>
#include <gpgme.h>

using namespace Kleo;
using namespace QGpgME;

static const char s_x509services_componentName[] = "gpgsm";
static const char s_x509services_entryName[] = "keyserver";

static const char s_x509services_legacy_componentName[] = "dirmngr";
static const char s_x509services_legacy_entryName[] = "LDAP Server";

static const char s_pgpservice_componentName[] = "dirmngr";
static const char s_pgpservice_entryName[] = "keyserver";

// legacy config entry used until GnuPG 2.2
static const char s_pgpservice_legacy_componentName[] = "gpg";
static const char s_pgpservice_legacy_entryName[] = "keyserver";

static const char s_timeout_componentName[] = "dirmngr";
static const char s_timeout_entryName[] = "ldaptimeout";

static const char s_maxitems_componentName[] = "dirmngr";
static const char s_maxitems_entryName[] = "max-replies";

class DirectoryServicesConfigurationPage::Private
{
    DirectoryServicesConfigurationPage *q = nullptr;

public:
    Private(DirectoryServicesConfigurationPage *q);

    void load();
    void save();
    void defaults();

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

    Kleo::LabelledWidget<QLineEdit> mOpenPGPKeyserverEdit;
    Kleo::DirectoryServicesWidget *mDirectoryServices = nullptr;
    Kleo::LabelledWidget<QTimeEdit> mTimeout;
    Kleo::LabelledWidget<QSpinBox> mMaxItems;

    QGpgME::CryptoConfigEntry *mX509ServicesEntry = nullptr;
    QGpgME::CryptoConfigEntry *mOpenPGPServiceEntry = nullptr;
    QGpgME::CryptoConfigEntry *mTimeoutConfigEntry = nullptr;
    QGpgME::CryptoConfigEntry *mMaxItemsConfigEntry = nullptr;

    QGpgME::CryptoConfig *mConfig = nullptr;
};

DirectoryServicesConfigurationPage::Private::Private(DirectoryServicesConfigurationPage *q)
{
    mConfig = QGpgME::cryptoConfig();
    auto glay = new QGridLayout(q);
    glay->setContentsMargins(0, 0, 0, 0);

    // OpenPGP keyserver
    int row = 0;
    {
        auto l = new QHBoxLayout{};
        l->setContentsMargins(0, 0, 0, 0);

        mOpenPGPKeyserverEdit.createWidgets(q);
        mOpenPGPKeyserverEdit.label()->setText(i18n("OpenPGP keyserver:"));
        l->addWidget(mOpenPGPKeyserverEdit.label());
        l->addWidget(mOpenPGPKeyserverEdit.widget());

        glay->addLayout(l, row, 0, 1, 3);
        connect(mOpenPGPKeyserverEdit.widget(), &QLineEdit::textEdited,
                q, [q]() { Q_EMIT q->changed(true); });
    }

    // X.509 servers
    if (Settings{}.cmsEnabled()) {
        ++row;
        auto groupBox = new QGroupBox{i18n("X.509 Directory Services"), q};
        auto groupBoxLayout = new QVBoxLayout{groupBox};

        if (gpgme_check_version("1.16.0")) {
            mDirectoryServices = new Kleo::DirectoryServicesWidget(q);
            if (QLayout *l = mDirectoryServices->layout()) {
                l->setContentsMargins(0, 0, 0, 0);
            }
            groupBoxLayout->addWidget(mDirectoryServices);
            connect(mDirectoryServices, SIGNAL(changed()), q, SLOT(changed()));
        } else {
            // QGpgME does not properly support keyserver flags for X.509 keyservers (added in GnuPG 2.2.28);
            // disable the configuration to prevent the configuration from being corrupted
            groupBoxLayout->addWidget(new QLabel{i18n("Configuration of directory services is not possible "
                                                      "because the used gpgme libraries are too old."), q});
        }

        glay->addWidget(groupBox, row, 0, 1, 3);
    }

    // LDAP timeout
    ++row;
    mTimeout.createWidgets(q);
    mTimeout.label()->setText(i18n("LDAP &timeout (minutes:seconds):"));
    mTimeout.widget()->setDisplayFormat(QStringLiteral("mm:ss"));
    connect(mTimeout.widget(), SIGNAL(timeChanged(QTime)), q, SLOT(changed()));
    glay->addWidget(mTimeout.label(), row, 0);
    glay->addWidget(mTimeout.widget(), row, 1);

    // Max number of items returned by queries
    ++row;
    mMaxItems.createWidgets(q);
    mMaxItems.label()->setText(i18n("&Maximum number of items returned by query:"));
    mMaxItems.widget()->setMinimum(0);
    connect(mMaxItems.widget(), SIGNAL(valueChanged(int)), q, SLOT(changed()));
    glay->addWidget(mMaxItems.label(), row, 0);
    glay->addWidget(mMaxItems.widget(), row, 1);

    glay->setRowStretch(++row, 1);
    glay->setColumnStretch(2, 1);
}

void DirectoryServicesConfigurationPage::Private::load()
{
    if (mDirectoryServices) {
        mDirectoryServices->clear();

        // gpgsm's keyserver option is not provided by very old gpgconf versions
        mX509ServicesEntry = configEntry(s_x509services_componentName, s_x509services_entryName,
                                        CryptoConfigEntry::ArgType_LDAPURL, ListValue, DoNotShowError);
        if (!mX509ServicesEntry) {
            mX509ServicesEntry = configEntry(s_x509services_legacy_componentName, s_x509services_legacy_entryName,
                                            CryptoConfigEntry::ArgType_LDAPURL, ListValue, DoShowError);
        }
        if (mX509ServicesEntry) {
            std::vector<KeyserverConfig> servers;
            const auto urls = mX509ServicesEntry->urlValueList();
            servers.reserve(urls.size());
            std::transform(std::begin(urls), std::end(urls), std::back_inserter(servers), [](const auto &url) { return KeyserverConfig::fromUrl(url); });
            mDirectoryServices->setKeyservers(servers);
            mDirectoryServices->setReadOnly(mX509ServicesEntry->isReadOnly());
        } else {
            mDirectoryServices->setDisabled(true);
        }
    }

    {
        // gpg prefers the deprecated keyserver option in gpg.conf over the keyserver option in dirmngr.conf;
        // therefore, we use the deprecated keyserver option if it is set or if the new option doesn't exist (gpg < 2.1.9)
        auto const newEntry = configEntry(s_pgpservice_componentName, s_pgpservice_entryName,
                                          CryptoConfigEntry::ArgType_String, SingleValue, DoNotShowError);
        auto const legacyEntry = configEntry(s_pgpservice_legacy_componentName, s_pgpservice_legacy_entryName,
                                             CryptoConfigEntry::ArgType_String, SingleValue, DoNotShowError);
        mOpenPGPServiceEntry = ((legacyEntry && legacyEntry->isSet()) || !newEntry) ? legacyEntry : newEntry;

        if (!mOpenPGPServiceEntry) {
            qCWarning(KLEOPATRA_LOG) << "Unknown or wrong typed config entries"
                << s_pgpservice_componentName << "/" << s_pgpservice_entryName
                << "and"
                << s_pgpservice_legacy_componentName << "/" << s_pgpservice_legacy_entryName;
        } else if (mOpenPGPServiceEntry == legacyEntry) {
            qCDebug(KLEOPATRA_LOG) << "Using config entry"
                << s_pgpservice_legacy_componentName << "/" << s_pgpservice_legacy_entryName;
        } else {
            qCDebug(KLEOPATRA_LOG) << "Using config entry"
                << s_pgpservice_componentName << "/" << s_pgpservice_entryName;
        }

        mOpenPGPKeyserverEdit.widget()->setText(mOpenPGPServiceEntry && mOpenPGPServiceEntry->isSet() ? mOpenPGPServiceEntry->stringValue() : QString());
        mOpenPGPKeyserverEdit.setEnabled(mOpenPGPServiceEntry && !mOpenPGPServiceEntry->isReadOnly());
#ifdef QGPGME_CRYPTOCONFIGENTRY_HAS_DEFAULT_VALUE
        if (newEntry && !newEntry->defaultValue().isNull()) {
            mOpenPGPKeyserverEdit.widget()->setPlaceholderText(newEntry->defaultValue().toString());
        } else
#endif
        {
            if (GpgME::engineInfo(GpgME::GpgEngine).engineVersion() < "2.1.16") {
                mOpenPGPKeyserverEdit.widget()->setPlaceholderText(QStringLiteral("hkp://keys.gnupg.net"));
            } else {
                mOpenPGPKeyserverEdit.widget()->setPlaceholderText(QStringLiteral("hkps://hkps.pool.sks-keyservers.net"));
            }
        }
    }

    // read LDAP timeout
    // first try to read the config entry as int (GnuPG 2.3)
    mTimeoutConfigEntry = configEntry(s_timeout_componentName, s_timeout_entryName, CryptoConfigEntry::ArgType_Int, SingleValue, DoNotShowError);
    if (!mTimeoutConfigEntry) {
        // if this fails, then try to read the config entry as unsigned int (GnuPG <= 2.2)
        mTimeoutConfigEntry = configEntry(s_timeout_componentName, s_timeout_entryName, CryptoConfigEntry::ArgType_UInt, SingleValue, DoShowError);
    }
    if (mTimeoutConfigEntry) {
        const int ldapTimeout = mTimeoutConfigEntry->argType() == CryptoConfigEntry::ArgType_Int ?
                                mTimeoutConfigEntry->intValue() :
                                static_cast<int>(mTimeoutConfigEntry->uintValue());
        const QTime time = QTime(0, 0, 0, 0).addSecs(ldapTimeout);
        //qCDebug(KLEOPATRA_LOG) <<"timeout:" << mTimeoutConfigEntry->uintValue() <<"  ->" << time;
        mTimeout.widget()->setTime(time);
    }
    mTimeout.setEnabled(mTimeoutConfigEntry && !mTimeoutConfigEntry->isReadOnly());

    // read max-replies config entry
    // first try to read the config entry as int (GnuPG 2.3)
    mMaxItemsConfigEntry = configEntry(s_maxitems_componentName, s_maxitems_entryName, CryptoConfigEntry::ArgType_Int, SingleValue, DoNotShowError);
    if (!mMaxItemsConfigEntry) {
        // if this fails, then try to read the config entry as unsigned int (GnuPG <= 2.2)
        mMaxItemsConfigEntry = configEntry(s_maxitems_componentName, s_maxitems_entryName, CryptoConfigEntry::ArgType_UInt, SingleValue, DoShowError);
    }
    if (mMaxItemsConfigEntry) {
        const int value = mMaxItemsConfigEntry->argType() == CryptoConfigEntry::ArgType_Int ?
                          mMaxItemsConfigEntry->intValue() :
                          static_cast<int>(mMaxItemsConfigEntry->uintValue());
        mMaxItems.widget()->blockSignals(true);   // KNumInput emits valueChanged from setValue!
        mMaxItems.widget()->setValue(value);
        mMaxItems.widget()->blockSignals(false);
    }
    mMaxItems.setEnabled(mMaxItemsConfigEntry && !mMaxItemsConfigEntry->isReadOnly());
}

namespace
{
void updateIntegerConfigEntry(QGpgME::CryptoConfigEntry *configEntry, int value) {
    if (!configEntry) {
        return;
    }
    if (configEntry->argType() == CryptoConfigEntry::ArgType_Int) {
        if (configEntry->intValue() != value) {
            configEntry->setIntValue(value);
        }
    } else {
        const auto newValue = static_cast<unsigned>(value);
        if (configEntry->uintValue() != newValue) {
            configEntry->setUIntValue(newValue);
        }
    }
}
}

void DirectoryServicesConfigurationPage::Private::save()
{
    if (mX509ServicesEntry && mDirectoryServices) {
        QList<QUrl> urls;
        const auto servers = mDirectoryServices->keyservers();
        urls.reserve(servers.size());
        std::transform(std::begin(servers), std::end(servers), std::back_inserter(urls), [](const auto &server) { return server.toUrl(); });
        mX509ServicesEntry->setURLValueList(urls);
    }

    if (mOpenPGPServiceEntry) {
        const auto keyserver = mOpenPGPKeyserverEdit.widget()->text().trimmed();
        if (keyserver.isEmpty()) {
            mOpenPGPServiceEntry->resetToDefault();
        } else {
            const auto keyserverUrl = keyserver.contains(QLatin1String{"://"}) ? keyserver : (QLatin1String{"hkps://"} + keyserver);
            mOpenPGPServiceEntry->setStringValue(keyserverUrl);
        }
    }

    const QTime time{mTimeout.widget()->time()};
    updateIntegerConfigEntry(mTimeoutConfigEntry, time.minute() * 60 + time.second());

    updateIntegerConfigEntry(mMaxItemsConfigEntry, mMaxItems.widget()->value());

    mConfig->sync(true);
}

void DirectoryServicesConfigurationPage::Private::defaults()
{
    // these guys don't have a default, to clear them:
    if (mX509ServicesEntry && !mX509ServicesEntry->isReadOnly()) {
        mX509ServicesEntry->setURLValueList(QList<QUrl>());
    }
    if (mOpenPGPServiceEntry && !mOpenPGPServiceEntry->isReadOnly()) {
        mOpenPGPServiceEntry->setStringValue(QString());
    }
    // these presumably have a default, use that one:
    if (mTimeoutConfigEntry && !mTimeoutConfigEntry->isReadOnly()) {
        mTimeoutConfigEntry->resetToDefault();
    }
    if (mMaxItemsConfigEntry && !mMaxItemsConfigEntry->isReadOnly()) {
        mMaxItemsConfigEntry->resetToDefault();
    }

    load();
}

// Find config entry for ldap servers. Implements runtime checks on the configuration option.
CryptoConfigEntry *DirectoryServicesConfigurationPage::Private::configEntry(const char *componentName,
        const char *entryName,
        CryptoConfigEntry::ArgType argType,
        EntryMultiplicity multiplicity,
        ShowError showError)
{
    CryptoConfigEntry *const entry = Kleo::getCryptoConfigEntry(mConfig, componentName, entryName);
    if (!entry) {
        if (showError == DoShowError) {
            KMessageBox::error(q, i18n("Backend error: gpgconf does not seem to know the entry for %1/%2", QLatin1String(componentName), QLatin1String(entryName)));
        }
        return nullptr;
    }
    if (entry->argType() != argType || entry->isList() != bool(multiplicity)) {
        if (showError == DoShowError) {
            KMessageBox::error(q, i18n("Backend error: gpgconf has wrong type for %1/%2: %3 %4", QLatin1String(componentName), QLatin1String(entryName), entry->argType(), entry->isList()));
        }
        return nullptr;
    }
    return entry;
}

DirectoryServicesConfigurationPage::DirectoryServicesConfigurationPage(QWidget *parent, const QVariantList &args)
    : KCModule{parent, args}
    , d{new Private{this}}
{
    load();
}

DirectoryServicesConfigurationPage::~DirectoryServicesConfigurationPage() = default;

void DirectoryServicesConfigurationPage::load()
{
    d->load();
}

void DirectoryServicesConfigurationPage::save()
{
    d->save();
}

void DirectoryServicesConfigurationPage::defaults()
{
    d->defaults();
}
