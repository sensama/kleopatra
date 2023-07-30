/*
    aboutdata.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2004 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aboutdata.h"

#include <version-kwatchgnupg.h>

#include <KLocalizedString>

#include <KLazyLocalizedString>

struct about_data {
    const KLazyLocalizedString name;
    const KLazyLocalizedString desc;
    const char *email;
    const char *web;
};

static const about_data authors[] = {
    {kli18n("Steffen Hansen"), kli18n("Original Author"), "hansen@kde.org", nullptr},
};

AboutData::AboutData()
    : KAboutData(QStringLiteral("kwatchgnupg"),
                 i18n("KWatchGnuPG"),
                 QStringLiteral(KWATCHGNUPG_VERSION_STRING),
                 i18n("GnuPG log viewer"),
                 KAboutLicense::GPL,
                 i18n("(c) 2004 Klar\xC3\xA4lvdalens Datakonsult AB\n"))
{
    using ::authors;
    // using ::credits;
    for (unsigned int i = 0; i < sizeof authors / sizeof *authors; ++i) {
        addAuthor(KLocalizedString(authors[i].name).toString(),
                  KLocalizedString(authors[i].desc).toString(),
                  QLatin1String(authors[i].email),
                  QLatin1String(authors[i].web));
    }
}
