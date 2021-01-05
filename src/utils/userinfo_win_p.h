/*
    utils/userinfo_win_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UTILS_USERINFO_WIN_P_H__
#define __KLEOPATRA_UTILS_USERINFO_WIN_P_H__

#include <windows.h>
#define SECURITY_WIN32
#include <secext.h> // For GetUserNameEx

#include <QString>

QString win_get_user_name(EXTENDED_NAME_FORMAT what);

bool win_user_is_elevated();

#endif // __KLEOPATRA_UTILS_USERINFO_WIN_P_H__
