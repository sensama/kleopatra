/*  smartcard/p15card.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Andre Heinecke <aheinecke@g10code.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "p15card.h"

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::SmartCard;

// static
const std::string P15Card::AppName = "p15";

P15Card::P15Card(const Card &card)
    : Card(card)
{
    setAppName(AppName);
}

std::string P15Card::appKeyFingerprint(const std::string &appKeyRef) const
{
    return cardInfo("KLEO-FPR-" + appKeyRef);
}

void P15Card::setManufacturer(const std::string &manufacturer)
{
    mManufacturer = manufacturer;
}

std::string P15Card::manufacturer() const
{
    return mManufacturer;
}

bool P15Card::operator == (const Card& rhs) const
{
    const P15Card *other = dynamic_cast<const P15Card *>(&rhs);
    if (!other) {
        return false;
    }

    return Card::operator ==(rhs)
        && mManufacturer == other->mManufacturer;
}
