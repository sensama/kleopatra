/*  smartcard/utils.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <string>

class QString;

namespace Kleo
{
namespace SmartCard
{

QString displayAppName(const std::string &appName);

} // namespace Smartcard
} // namespace Kleopatra

