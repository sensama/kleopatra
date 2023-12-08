/* -*- mode: c++; c-basic-offset:4 -*-
    conf/smartcardconfigpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "smartcardconfigpage.h"

#include <Libkleo/Compat>
#include <Libkleo/ReaderPortSelection>

#include <KLocalizedString>

#include <QGpgME/CryptoConfig>
#include <QGpgME/Protocol>

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

using namespace Kleo;
using namespace Kleo::Config;
using namespace QGpgME;

class SmartCardConfigurationPage::Private
{
public:
    Private(SmartCardConfigurationPage *q);

    static CryptoConfigEntry *readerPortConfigEntry(const CryptoConfig *config = nullptr);

public:
    ReaderPortSelection *const mReaderPort;
};

SmartCardConfigurationPage::Private::Private(SmartCardConfigurationPage *qq)
#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
    : mReaderPort{new ReaderPortSelection{qq}}
#else
    : mReaderPort{new ReaderPortSelection{qq->widget()}}
#endif
{
}

// static
CryptoConfigEntry *SmartCardConfigurationPage::Private::readerPortConfigEntry(const CryptoConfig *config)
{
    if (!config) {
        config = QGpgME::cryptoConfig();
    }
    return Kleo::getCryptoConfigEntry(config, "scdaemon", "reader-port");
}

#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
SmartCardConfigurationPage::SmartCardConfigurationPage(QWidget *parent, const QVariantList &args)
    : KCModule{parent, args}
#else
SmartCardConfigurationPage::SmartCardConfigurationPage(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KCModule(parent, data, args)
#endif
    , d{std::make_unique<Private>(this)}
{
#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
    auto mainLayout = new QVBoxLayout{this};
#else
    auto mainLayout = new QVBoxLayout{widget()};
#endif

    {
        auto l = new QHBoxLayout{};
        l->setContentsMargins(0, 0, 0, 0);

#if KCMUTILS_VERSION < QT_VERSION_CHECK(5, 240, 0)
        auto label = new QLabel{i18n("Smart card reader to use:"), this};
#else
        auto label = new QLabel{i18n("Smart card reader to use:"), widget()};
#endif
        label->setBuddy(d->mReaderPort);

        l->addWidget(label);
        l->addWidget(d->mReaderPort, 1);

        mainLayout->addLayout(l);

        connect(d->mReaderPort, &ReaderPortSelection::valueChanged, this, &SmartCardConfigurationPage::markAsChanged);
    }

    mainLayout->addStretch();

    load();
}

SmartCardConfigurationPage::~SmartCardConfigurationPage() = default;

void SmartCardConfigurationPage::load()
{
    const auto *const entry = d->readerPortConfigEntry();
    if (entry) {
        d->mReaderPort->setEnabled(!entry->isReadOnly());
        d->mReaderPort->setValue(entry->stringValue());
    } else {
        d->mReaderPort->setEnabled(false);
        d->mReaderPort->setValue(i18n("Cannot be configured with Kleopatra"));
    }
}

void SmartCardConfigurationPage::save()
{
    auto config = QGpgME::cryptoConfig();

    auto const entry = d->readerPortConfigEntry(config);
    if (entry && !entry->isReadOnly()) {
        entry->setStringValue(d->mReaderPort->value());
    }

    config->sync(true);
}

void SmartCardConfigurationPage::defaults()
{
    const auto *const entry = d->readerPortConfigEntry();
    if (entry && !entry->isReadOnly()) {
        d->mReaderPort->setValue({});
    }
}
