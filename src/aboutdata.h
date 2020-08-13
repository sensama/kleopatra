/*
    aboutdata.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2001, 2002, 2004 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __ABOUTDATA_H__
#define __ABOUTDATA_H__

#include <KAboutData>

class AboutData : public KAboutData
{
public:
    AboutData();
};

class AboutGpg4WinData : public KAboutData
{
public:
    AboutGpg4WinData();
};

#endif // __ABOUTDATA_H__
