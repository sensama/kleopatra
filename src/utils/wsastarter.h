/* -*- mode: c++; c-basic-offset:4 -*-
    utils/wsastarter.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UTILS_WSASTARTER_H__
#define __KLEOPATRA_UTILS_WSASTARTER_H__

namespace Kleo
{

struct WSAStarter {
    const int startupError;

    WSAStarter();
    ~WSAStarter();
};

} // namespace Kleo

#endif // __KLEOPATRA_UTILS_WSASTARTER_H__
