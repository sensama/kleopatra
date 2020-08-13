/*
    aboutdata.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2004 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <version-kwatchgnupg.h>
#include "aboutdata.h"

#include <KLocalizedString>


struct about_data {
    const char *name;
    const char *desc;
    const char *email;
    const char *web;
};

static const about_data authors[] = {
    { I18N_NOOP("Steffen Hansen"), I18N_NOOP("Original Author"), "hansen@kde.org", nullptr },
};

AboutData::AboutData()
    : KAboutData(QStringLiteral("kwatchgnupg"), i18n("KWatchGnuPG"),
                 QStringLiteral(KWATCHGNUPG_VERSION_STRING), i18n("GnuPG log viewer"), KAboutLicense::GPL,
                 i18n("(c) 2004 Klar\xC3\xA4lvdalens Datakonsult AB\n"))
{
    using ::authors;
    //using ::credits;
    for (unsigned int i = 0; i < sizeof authors / sizeof * authors; ++i)
        addAuthor(i18n(authors[i].name), i18n(authors[i].desc),
                  QLatin1String(authors[i].email), QLatin1String(authors[i].web));
}
