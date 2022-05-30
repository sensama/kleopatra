/* -*- mode: c++; c-basic-offset:4 -*-
    utils/qt-cxx20-compat.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <Qt>

// define some bitwise operators to avoid warning that bitwise operation between
// different enumeration types is deprecated
inline int operator|(Qt::Modifier modifier, Qt::Key key)
{
    return static_cast<int>(modifier) | static_cast<int>(key);
}
