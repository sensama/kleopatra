/* -*- mode: c++; c-basic-offset:4 -*-
    utils/gpgme-compat.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2023 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <config-kleopatra.h>

#include <QGpgME/Debug>

#if 0
// intentionally left blank
#else
#include <sstream>

namespace QGpgME
{
template<class GpgMEClass>
std::string toLogStringX(const GpgMEClass &object)
{
    std::stringstream stream;
    stream << object;
    return stream.str();
}
}
#endif
