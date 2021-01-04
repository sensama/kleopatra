/*
    utils/userinfo.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UTILS_USERINFO_H__
#define __KLEOPATRA_UTILS_USERINFO_H__

class QString;

namespace Kleo
{
    QString userFullName();

    QString userEmailAddress();
}

#endif // __KLEOPATRA_UTILS_USERINFO_H__
