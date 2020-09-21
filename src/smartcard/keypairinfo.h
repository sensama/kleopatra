/*  smartcard/keypairinfo.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef SMARTCARD_KEYPAIRINFO_H
#define SMARTCARD_KEYPAIRINFO_H

#include <string>

namespace Kleo
{
namespace SmartCard
{
struct KeyPairInfo {
    static KeyPairInfo fromStatusLine(const std::string &s);

    std::string grip;
    std::string keyRef;
    std::string usage;
    std::string keyTime;
    std::string algorithm;
};
} // namespace Smartcard
} // namespace Kleopatra

#endif // SMARTCARD_PIVCARD_H
