/*

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2001, 2002, 2004 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <Libkleo/GnuPG>

#define WATCHGNUPGBINARY QStringLiteral("watchgnupg")
#define WATCHGNUPGSOCKET QString( Kleo::gnupgHomeDirectory() + QLatin1String("/log-socket") )


