/* -*- mode: c++; c-basic-offset:4 -*-
    conf/dirservconfigpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2004, 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "dirservconfigpage.h"

#include "compat.h"

#include <Libkleo/Compat>
#include <Libkleo/DirectoryServicesWidget>
#include <Libkleo/CryptoConfigModule>

#include <QGpgME/Protocol>

#include <KMessageBox>
#include <KLocalizedString>
#include "kleopatra_debug.h"
#include <KConfig>
#include <QSpinBox>

#include <QLabel>
#include <QCheckBox>
#include <QLayout>
#include <QTimeEdit>

using namespace Kleo;
using namespace QGpgME;

#if 0 // disabled, since it is apparently confusing
// For sync'ing kabldaprc
class KABSynchronizer
{
public:
    KABSynchronizer()
        : mConfig("kabldaprc")
    {
        mConfig.setGroup("LDAP");
    }

    KUrl::List readCurrentList() const
    {

        KUrl::List lst;
        // stolen from kabc/ldapclient.cpp
        const uint numHosts = mConfig.readEntry("NumSelectedHosts");
        for (uint j = 0; j < numHosts; j++) {
            const QString num = QString::number(j);

            KUrl url;
            url.setProtocol("ldap");
            url.setPath("/");   // workaround KUrl parsing bug
            const QString host = mConfig.readEntry(QString("SelectedHost") + num).trimmed();
            url.setHost(host);

            const int port = mConfig.readEntry(QString("SelectedPort") + num);
            if (port != 0) {
                url.setPort(port);
            }

            const QString base = mConfig.readEntry(QString("SelectedBase") + num).trimmed();
            url.setQuery(base);

            const QString bindDN = mConfig.readEntry(QString("SelectedBind") + num).trimmed();
            url.setUser(bindDN);

            const QString pwdBindDN = mConfig.readEntry(QString("SelectedPwdBind") + num).trimmed();
            url.setPass(pwdBindDN);
            lst.append(url);
        }
        return lst;
    }

    void writeList(const KUrl::List &lst)
    {

        mConfig.writeEntry("NumSelectedHosts", lst.count());

        KUrl::List::const_iterator it = lst.begin();
        KUrl::List::const_iterator end = lst.end();
        unsigned j = 0;
        for (; it != end; ++it, ++j) {
            const QString num = QString::number(j);
            KUrl url = *it;

            Q_ASSERT(url.scheme() == "ldap");
            mConfig.writeEntry(QString("SelectedHost") + num, url.host());
            mConfig.writeEntry(QString("SelectedPort") + num, url.port());

            // KUrl automatically encoded the query (e.g. for spaces inside it),
            // so decode it before writing it out
            const QString base = KUrl::decode_string(url.query().mid(1));
            mConfig.writeEntry(QString("SelectedBase") + num, base);
            mConfig.writeEntry(QString("SelectedBind") + num, url.user());
            mConfig.writeEntry(QString("SelectedPwdBind") + num, url.pass());
        }
        mConfig.sync();
    }

private:
    KConfig mConfig;
};

#endif

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

#ifdef NOT_USEFUL_CURRENTLY
static const char s_addnewservers_componentName[] = "dirmngr";
static const char s_addnewservers_entryName[] = "add-servers";
#endif

DirectoryServicesConfigurationPage::DirectoryServicesConfigurationPage(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
{
    mConfig = QGpgME::cryptoConfig();
    QGridLayout *glay = new QGridLayout(this);
    glay->setContentsMargins(0, 0, 0, 0);

    int row = 0;
    mWidget = new Kleo::DirectoryServicesWidget(this);
    if (QLayout *l = mWidget->layout()) {
        l->setContentsMargins(0, 0, 0, 0);
    }
    glay->addWidget(mWidget, row, 0, 1, 3);
    connect(mWidget, SIGNAL(changed()), this, SLOT(changed()));

    // LDAP timeout
    ++row;
    QLabel *label = new QLabel(i18n("LDAP &timeout (minutes:seconds):"), this);
    mTimeout = new QTimeEdit(this);
    mTimeout->setDisplayFormat(QStringLiteral("mm:ss"));
    connect(mTimeout, SIGNAL(timeChanged(QTime)), this, SLOT(changed()));
    label->setBuddy(mTimeout);
    glay->addWidget(label, row, 0);
    glay->addWidget(mTimeout, row, 1);

    // Max number of items returned by queries
    ++row;
    mMaxItemsLabel = new QLabel(i18n("&Maximum number of items returned by query:"), this);
    mMaxItems = new QSpinBox(this);
    mMaxItems->setMinimum(0);
    mMaxItemsLabel->setBuddy(mMaxItems);
    connect(mMaxItems, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    glay->addWidget(mMaxItemsLabel, row, 0);
    glay->addWidget(mMaxItems, row, 1);

#ifdef NOT_USEFUL_CURRENTLY
    ++row
    mAddNewServersCB = new QCheckBox(i18n("Automatically add &new servers discovered in CRL distribution points"), this);
    connect(mAddNewServersCB, SIGNAL(clicked()), this, SLOT(changed()));
    glay->addWidget(mAddNewServersCB, row, 0, 1, 3);
#endif

    glay->setRowStretch(++row, 1);
    glay->setColumnStretch(2, 1);

    load();
}

static QList<QUrl> string2urls(const QString &str)
{
    QList<QUrl> ret;
    if (str.isEmpty()) {
        return ret;
    }
    ret << QUrl::fromEncoded(str.toLocal8Bit());
    return ret;
}

void DirectoryServicesConfigurationPage::load()
{
    mWidget->clear();

    // gpgsm's keyserver option is not provided by very old gpgconf versions
    if ((mX509ServicesEntry = configEntry(s_x509services_componentName, s_x509services_entryName,
                                          CryptoConfigEntry::ArgType_LDAPURL, ListValue, DoNotShowError))) {
        mWidget->addX509Services(mX509ServicesEntry->urlValueList());
    } else if ((mX509ServicesEntry = configEntry(s_x509services_legacy_componentName, s_x509services_legacy_entryName,
                                                 CryptoConfigEntry::ArgType_LDAPURL, ListValue, DoShowError))) {
        mWidget->addX509Services(mX509ServicesEntry->urlValueList());
    }
    mWidget->setX509ReadOnly(mX509ServicesEntry && mX509ServicesEntry->isReadOnly());

    {
        auto *const newEntry = configEntry(s_pgpservice_componentName, s_pgpservice_entryName,
                                           CryptoConfigEntry::ArgType_String, SingleValue, DoNotShowError);
        auto *const legacyEntry = configEntry(s_pgpservice_legacy_componentName, s_pgpservice_legacy_entryName,
                                              CryptoConfigEntry::ArgType_String, SingleValue, DoNotShowError);
        mOpenPGPServiceEntry = newEntry ? newEntry : legacyEntry;

        QString stringValue;
        if (newEntry && legacyEntry && !newEntry->isSet() && legacyEntry->isSet()) {
            // use value of legacy entry if value of new entry is unset
            qCDebug(KLEOPATRA_LOG) << "Using value of legacy entry for config entry"
                << s_pgpservice_componentName << "/" << s_pgpservice_entryName;
            stringValue = legacyEntry->stringValue();
        } else if (mOpenPGPServiceEntry) {
            stringValue = mOpenPGPServiceEntry->stringValue();
        } else {
            qCWarning(KLEOPATRA_LOG) << "Unknown or wrong typed config entry"
                << s_pgpservice_componentName << "/" << s_pgpservice_entryName;
        }
        mWidget->addOpenPGPServices(string2urls(parseKeyserver(stringValue).url));
        mWidget->setOpenPGPReadOnly(mOpenPGPServiceEntry && mOpenPGPServiceEntry->isReadOnly());
    }

    if (mX509ServicesEntry)
        if (mOpenPGPServiceEntry) {
            mWidget->setAllowedProtocols(DirectoryServicesWidget::AllProtocols);
        } else {
            mWidget->setAllowedProtocols(DirectoryServicesWidget::X509Protocol);
        }
    else if (mOpenPGPServiceEntry) {
        mWidget->setAllowedProtocols(DirectoryServicesWidget::OpenPGPProtocol);
    } else {
        mWidget->setDisabled(true);
    }

    DirectoryServicesWidget::Protocols readOnlyProtocols;
    if (mX509ServicesEntry && mX509ServicesEntry->isReadOnly()) {
        readOnlyProtocols = DirectoryServicesWidget::X509Protocol;
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
        mTimeout->setTime(time);
    }

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
        mMaxItems->blockSignals(true);   // KNumInput emits valueChanged from setValue!
        mMaxItems->setValue(value);
        mMaxItems->blockSignals(false);
    }
    const bool maxItemsEnabled = mMaxItemsConfigEntry && !mMaxItemsConfigEntry->isReadOnly();
    mMaxItems->setEnabled(maxItemsEnabled);
    mMaxItemsLabel->setEnabled(maxItemsEnabled);

#ifdef NOT_USEFUL_CURRENTLY
    mAddNewServersConfigEntry = configEntry(s_addnewservers_componentName, s_addnewservers_groupName, s_addnewservers_entryName, CryptoConfigEntry::ArgType_None, SingleValue, DoShowError);
    if (mAddNewServersConfigEntry) {
        mAddNewServersCB->setChecked(mAddNewServersConfigEntry->boolValue());
    }
#endif
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
        const unsigned int newValue = static_cast<unsigned>(value);
        if (configEntry->uintValue() != newValue) {
            configEntry->setUIntValue(newValue);
        }
    }
}
}

void DirectoryServicesConfigurationPage::save()
{
    if (mX509ServicesEntry) {
        mX509ServicesEntry->setURLValueList(mWidget->x509Services());
    }

    if (mOpenPGPServiceEntry) {
        const QList<QUrl> serv = mWidget->openPGPServices();
        if (serv.empty()) {
            mOpenPGPServiceEntry->setStringValue(QString());
        } else {
            ParsedKeyserver pks = parseKeyserver(mOpenPGPServiceEntry->stringValue());
            pks.url = serv.front().url();
            mOpenPGPServiceEntry->setStringValue(assembleKeyserver(pks));
        }
    }

    const QTime time{mTimeout->time()};
    updateIntegerConfigEntry(mTimeoutConfigEntry, time.minute() * 60 + time.second());

    updateIntegerConfigEntry(mMaxItemsConfigEntry, mMaxItems->value());

#ifdef NOT_USEFUL_CURRENTLY
    if (mAddNewServersConfigEntry && mAddNewServersConfigEntry->boolValue() != mAddNewServersCB->isChecked()) {
        mAddNewServersConfigEntry->setBoolValue(mAddNewServersCB->isChecked());
    }
#endif

    mConfig->sync(true);

#if 0
    // Also write the LDAP URLs to kabldaprc so that they are used by kaddressbook
    KABSynchronizer sync;
    const KUrl::List toAdd = mWidget->urlList();
    KUrl::List currentList = sync.readCurrentList();

    KUrl::List::const_iterator it = toAdd.begin();
    KUrl::List::const_iterator end = toAdd.end();
    for (; it != end; ++it) {
        // check if the URL is already in currentList
        if (currentList.find(*it) == currentList.end())
            // if not, add it
        {
            currentList.append(*it);
        }
    }
    sync.writeList(currentList);
#endif
}

void DirectoryServicesConfigurationPage::defaults()
{
    // these guys don't have a default, to clear them:
    if (mX509ServicesEntry) {
        mX509ServicesEntry->setURLValueList(QList<QUrl>());
    }
    if (mOpenPGPServiceEntry) {
        mOpenPGPServiceEntry->setStringValue(QString());
    }
    // these presumably have a default, use that one:
    if (mTimeoutConfigEntry) {
        mTimeoutConfigEntry->resetToDefault();
    }
    if (mMaxItemsConfigEntry) {
        mMaxItemsConfigEntry->resetToDefault();
    }
#ifdef NOT_USEFUL_CURRENTLY
    if (mAddNewServersConfigEntry) {
        mAddNewServersConfigEntry->resetToDefault();
    }
#endif
    load();
}

extern "C"
{
    Q_DECL_EXPORT KCModule *create_kleopatra_config_dirserv(QWidget *parent = nullptr, const QVariantList &args = QVariantList())
    {
        DirectoryServicesConfigurationPage *page =
            new DirectoryServicesConfigurationPage(parent, args);
        page->setObjectName(QStringLiteral("kleopatra_config_dirserv"));
        return page;
    }
}

// Find config entry for ldap servers. Implements runtime checks on the configuration option.
CryptoConfigEntry *DirectoryServicesConfigurationPage::configEntry(const char *componentName,
        const char *entryName,
        CryptoConfigEntry::ArgType argType,
        EntryMultiplicity multiplicity,
        ShowError showError)
{
    CryptoConfigEntry *const entry = Kleo::getCryptoConfigEntry(mConfig, componentName, entryName);
    if (!entry) {
        if (showError == DoShowError) {
            KMessageBox::error(this, i18n("Backend error: gpgconf does not seem to know the entry for %1/%2", QLatin1String(componentName), QLatin1String(entryName)));
        }
        return nullptr;
    }
    if (entry->argType() != argType || entry->isList() != bool(multiplicity)) {
        if (showError == DoShowError) {
            KMessageBox::error(this, i18n("Backend error: gpgconf has wrong type for %1/%2: %3 %4", QLatin1String(componentName), QLatin1String(entryName), entry->argType(), entry->isList()));
        }
        return nullptr;
    }
    return entry;
}
