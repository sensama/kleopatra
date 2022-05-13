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

#include <QLocale>

#include <KLocalizedString>
#include <KLazyLocalizedString>
static const char kleopatra_version[] = KLEOPATRA_VERSION_STRING;

struct about_data {
    const KLazyLocalizedString name;
    const KLazyLocalizedString desc;
    const char *email;
    const char *web;
};

static const about_data authors[] = {
    {kli18n("Andre Heinecke"), kli18n("Current Maintainer"),
     "aheinecke@gnupg.org", nullptr},
    {kli18n("Marc Mutz"), kli18n("Former Maintainer"), "mutz@kde.org", nullptr},
    {kli18n("Steffen Hansen"), kli18n("Former Maintainer"), "hansen@kde.org",
     nullptr},
    {kli18n("Matthias Kalle Dalheimer"), kli18n("Original Author"),
     "kalle@kde.org", nullptr},
};

static const about_data credits[] = {
    {kli18n("David Faure"),
     kli18n("Backend configuration framework, KIO integration"),
     "faure@kde.org", nullptr},
    {kli18n("Michel Boyer de la Giroday"),
     kli18n("Key-state dependent colors and fonts in the certificates list"),
     "michel@klaralvdalens-datakonsult.se", nullptr},
    {kli18n("Thomas Moenicke"), kli18n("Artwork"), "tm@php-qt.org", nullptr},
    {kli18n("Frank Osterfeld"),
     kli18n("Resident gpgme/win wrangler, UI Server commands and dialogs"),
     "osterfeld@kde.org", nullptr},
    {kli18n("Karl-Heinz Zimmer"),
     kli18n("DN display ordering support, infrastructure"), "khz@kde.org",
     nullptr},
    {kli18n("Laurent Montel"), kli18n("Qt5 port, general code maintenance"),
     "montel@kde.org", nullptr},
};

AboutData::AboutData()
    : KAboutData(QStringLiteral("kleopatra"),
      (Kleo::brandingWindowTitle().isEmpty() ?
       i18n("Kleopatra") : Kleo::brandingWindowTitle()),
#ifdef Q_OS_WIN
                 Kleo::gpg4winVersion(),
                 Kleo::gpg4winDescription(),
#else
                 QLatin1String(kleopatra_version),
                 i18n("Certificate Manager and Unified Crypto GUI"),
#endif
                 KAboutLicense::GPL,
                 i18n("(c) 2002 Steffen\u00A0Hansen, Matthias\u00A0Kalle\u00A0Dalheimer, Klar\u00E4lvdalens\u00A0Datakonsult\u00A0AB\n"
                      "(c) 2004, 2007, 2008, 2009 Marc\u00A0Mutz, Klar\u00E4lvdalens\u00A0Datakonsult\u00A0AB") //
                 + QLatin1Char('\n') //
                 + i18n("(c) 2016-2018 Intevation GmbH") //
                 + QLatin1Char('\n') //
                 + i18n("(c) 2010-%1 The Kleopatra developers, g10 Code GmbH", QStringLiteral("2023"))
                 )
{
#ifdef Q_OS_WIN
    setOtherText(Kleo::gpg4winLongDescription());
#endif
    using ::authors;
    using ::credits;
    for (unsigned int i = 0; i < sizeof authors / sizeof * authors; ++i) {
        addAuthor(KLocalizedString(authors[i].name).toString(), KLocalizedString(authors[i].desc).toString(),
                  QLatin1String(authors[i].email), QLatin1String(authors[i].web));
    }
    for (unsigned int i = 0; i < sizeof credits / sizeof * credits; ++i) {
        addCredit(KLocalizedString(credits[i].name).toString(), KLocalizedString(credits[i].desc).toString(),
                  QLatin1String(credits[i].email), QLatin1String(credits[i].web));
    }

    /* For Linux it is possible that kleo is shipped as part
     * of a Gpg4win based Appimage with according about data. */
    if (Kleo::gpg4winSignedversion()) {
        setVersion(Kleo::gpg4winVersion().toUtf8());
        setShortDescription(Kleo::gpg4winDescription());
        setOtherText(Kleo::gpg4winLongDescription());

        /* Bug reporting page is only available in german and english */
        if (QLocale().uiLanguages().first().startsWith(QStringLiteral("de"))) {
            setBugAddress("https://gnupg.com/vsd/report.de.html");
        } else {
            setBugAddress("https://gnupg.com/vsd/report.html");
        }
    } else {
#ifdef Q_OS_WIN
       setBugAddress("https://dev.gnupg.org/u/rgpg4win");
#endif
    }

    const auto backendVersions = Kleo::backendVersionInfo();
    if (!backendVersions.empty()) {
        setOtherText(i18nc("Preceeds a list of applications/libraries used by Kleopatra", "Uses:") //
                     + QLatin1String{"<ul><li>"} //
                     + backendVersions.join(QLatin1String{"</li><li>"}) //
                     + QLatin1String{"</li></ul>"} //
                     + otherText());
    }
}
