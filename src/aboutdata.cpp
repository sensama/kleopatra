/*
    aboutdata.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2001, 2002, 2004 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>
#include <version-kleopatra.h>

#include "aboutdata.h"

#include <Libkleo/GnuPG>

#include <KLocalizedString>
#include <ki18n_version.h>
#if KI18N_VERSION >= QT_VERSION_CHECK(5, 89, 0)
#include <KLazyLocalizedString>
#undef I18N_NOOP
#define I18N_NOOP kli18n
#endif
static const char kleopatra_version[] = KLEOPATRA_VERSION_STRING;

struct about_data {
#if KI18N_VERSION < QT_VERSION_CHECK(5, 89, 0)
    const char *name;
    const char *desc;
#else
    const KLazyLocalizedString name;
    const KLazyLocalizedString desc;
#endif
    const char *email;
    const char *web;
};

static const about_data authors[] = {
    { I18N_NOOP("Andre Heinecke"), I18N_NOOP("Current Maintainer"), "aheinecke@gnupg.org", nullptr },
    { I18N_NOOP("Marc Mutz"), I18N_NOOP("Former Maintainer"), "mutz@kde.org", nullptr },
    { I18N_NOOP("Steffen Hansen"), I18N_NOOP("Former Maintainer"), "hansen@kde.org", nullptr },
    { I18N_NOOP("Matthias Kalle Dalheimer"), I18N_NOOP("Original Author"), "kalle@kde.org", nullptr },
};

static const about_data credits[] = {
    {
        I18N_NOOP("David Faure"),
        I18N_NOOP("Backend configuration framework, KIO integration"),
        "faure@kde.org", nullptr
    },
    {
        I18N_NOOP("Michel Boyer de la Giroday"),
        I18N_NOOP("Key-state dependent colors and fonts in the certificates list"),
        "michel@klaralvdalens-datakonsult.se", nullptr
    },
    {
        I18N_NOOP("Thomas Moenicke"),
        I18N_NOOP("Artwork"),
        "tm@php-qt.org", nullptr
    },
    {
        I18N_NOOP("Frank Osterfeld"),
        I18N_NOOP("Resident gpgme/win wrangler, UI Server commands and dialogs"),
        "osterfeld@kde.org", nullptr
    },
    {
        I18N_NOOP("Karl-Heinz Zimmer"),
        I18N_NOOP("DN display ordering support, infrastructure"),
        "khz@kde.org", nullptr
    },
    {
        I18N_NOOP("Laurent Montel"),
        I18N_NOOP("Qt5 port, general code maintenance"),
        "montel@kde.org", nullptr
    },
};

AboutData::AboutData()
    : KAboutData(QStringLiteral("kleopatra"), i18n("Kleopatra"),
#ifdef Q_OS_WIN
                 Kleo::gpg4winVersion(),
                 Kleo::gpg4winDescription(),
#else
                 QLatin1String(kleopatra_version),
                 i18n("Certificate Manager and Unified Crypto GUI"),
#endif
                 KAboutLicense::GPL,
                 i18n("(c) 2002 Steffen\u00A0Hansen, Matthias\u00A0Kalle\u00A0" "Dalheimer, Klar\u00E4lvdalens\u00A0" "Datakonsult\u00A0" "AB\n"
                      "(c) 2004, 2007, 2008, 2009 Marc\u00A0Mutz, Klar\u00E4lvdalens\u00A0" "Datakonsult\u00A0" "AB") +
                 QLatin1Char('\n') + i18n("(c) 2010-2021 The Kleopatra developers, g10 Code GmbH, Intevation GmbH")
#ifdef Q_OS_WIN
                 , Kleo::gpg4winLongDescription()
#endif
                 )
{
    using ::authors;
    using ::credits;
#if KI18N_VERSION < QT_VERSION_CHECK(5, 89, 0)
    for (unsigned int i = 0; i < sizeof authors / sizeof * authors; ++i) {
        addAuthor(i18n(authors[i].name), i18n(authors[i].desc),
                  QLatin1String(authors[i].email), QLatin1String(authors[i].web));
    }
    for (unsigned int i = 0; i < sizeof credits / sizeof * credits; ++i) {
        addCredit(i18n(credits[i].name), i18n(credits[i].desc),
                  QLatin1String(credits[i].email), QLatin1String(credits[i].web));
    }
#else
    for (unsigned int i = 0; i < sizeof authors / sizeof * authors; ++i) {
        addAuthor(KLocalizedString(authors[i].name).toString(), KLocalizedString(authors[i].desc).toString(),
                  QLatin1String(authors[i].email), QLatin1String(authors[i].web));
    }
    for (unsigned int i = 0; i < sizeof credits / sizeof * credits; ++i) {
        addCredit(KLocalizedString(credits[i].name).toString(), KLocalizedString(credits[i].desc).toString(),
                  QLatin1String(credits[i].email), QLatin1String(credits[i].web));
    }
#endif


    const auto backendVersions = Kleo::backendVersionInfo();
    if (!backendVersions.empty()) {
        setOtherText(i18nc("Preceeds a list of applications/libraries used by Kleopatra",
                           "Uses:") +
                     QLatin1String{"<ul><li>"} +
                     backendVersions.join(QLatin1String{"</li><li>"}) +
                     QLatin1String{"</li></ul>"} +
                     otherText());
    }
#ifdef Q_OS_WIN
    setBugAddress("https://dev.gnupg.org/u/rgpg4win");
#endif
}
