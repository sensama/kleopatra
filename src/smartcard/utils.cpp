/*  smartcard/utils.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils.h"

#include "netkeycard.h"
#include "openpgpcard.h"
#include "pivcard.h"

#include <KLocalizedString>

#include <QString>

using namespace Kleo::SmartCard;

QString Kleo::SmartCard::displayAppName(const std::string &appName)
{
    if (appName == NetKeyCard::AppName) {
        return i18nc("proper name of a type of smartcard", "NetKey");
    } else if (appName == OpenPGPCard::AppName) {
        return i18nc("proper name of a type of smartcard", "OpenPGP");
    } else if (appName == PIVCard::AppName) {
        return i18nc("proper name of a type of smartcard", "PIV");
    } else {
        return QString::fromStdString(appName);
    }
}
