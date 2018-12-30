/*
    aboutdata.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2001,2002,2004 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config-kleopatra.h>
#include <version-kleopatra.h>

#include "aboutdata.h"

#include <KLocalizedString>

static const char kleopatra_version[] = KLEOPATRA_VERSION_STRING;
static const char description[] = I18N_NOOP("Certificate Manager and Unified Crypto GUI");

struct about_data {
    const char *name;
    const char *desc;
    const char *email;
    const char *web;
};

static const about_data authors[] = {
    { "Andre Heinecke", I18N_NOOP("Current Maintainer"), "aheinecke@gnupg.org", nullptr },
    { "Marc Mutz", I18N_NOOP("Former Maintainer"), "mutz@kde.org", nullptr },
    { "Steffen Hansen", I18N_NOOP("Former Maintainer"), "hansen@kde.org", nullptr },
    { "Matthias Kalle Dalheimer", I18N_NOOP("Original Author"), "kalle@kde.org", nullptr },
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
                 QLatin1String(kleopatra_version), i18n(description), KAboutLicense::GPL,
                 i18n("(c) 2002 Steffen\u00A0Hansen, Matthias\u00A0Kalle\u00A0" "Dalheimer, Klar\u00E4lvdalens\u00A0" "Datakonsult\u00A0" "AB\n"
                      "(c) 2004, 2007, 2008, 2009 Marc\u00A0Mutz, Klar\u00E4lvdalens\u00A0" "Datakonsult\u00A0" "AB") +
                 QLatin1Char('\n') + i18n("(c) 2010-2019 The Kleopatra developers")
#ifdef Q_OS_WIN
                 , i18n("<a href=https://www.gpg4win.org>Visit the Gpg4win homepage</a>")
#endif
                 )
{
    using ::authors;
    using ::credits;
    for (unsigned int i = 0; i < sizeof authors / sizeof * authors; ++i) {
        addAuthor(i18n(authors[i].name), i18n(authors[i].desc),
                  QLatin1String(authors[i].email), QLatin1String(authors[i].web));
    }
    for (unsigned int i = 0; i < sizeof credits / sizeof * credits; ++i) {
        addCredit(i18n(credits[i].name), i18n(credits[i].desc),
                  QLatin1String(credits[i].email), QLatin1String(credits[i].web));
    }

#ifdef Q_OS_WIN
    setBugAddress("https://dev.gnupg.org/u/rgpg4win");
#endif
}
