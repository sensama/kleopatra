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

void P15Card::setCardInfo(const std::vector< std::pair<std::string, std::string> > &infos)
{
    // XXX: This is a copy of OpenPGPCard::setCardInfo
    qCDebug(KLEOPATRA_LOG) << "Card" << serialNumber().c_str() << "info:";
    for (const auto &pair: infos) {
        qCDebug(KLEOPATRA_LOG) << pair.first.c_str() << ":" << pair.second.c_str();
        if (parseCardInfo(pair.first, pair.second)) {
            continue;
        }
        if (pair.first == "KEY-FPR") {
            const auto values = QString::fromStdString(pair.second).split(QLatin1Char(' '));
            if (values.size() < 2) {
                qCWarning(KLEOPATRA_LOG) << "Invalid entry.";
                setStatus(Card::CardError);
                continue;
            }
            const auto keyNumber = values[0];
            const std::string keyRef = "OPENPGP." + keyNumber.toStdString();
            const auto fpr = values[1].toStdString();
            if (keyNumber == QLatin1Char('1') || keyNumber == QLatin1Char('2') || keyNumber == QLatin1Char('3')) {
                addCardInfo("KLEO-FPR-" + keyRef, fpr);
            } else {
                // Maybe more keyslots in the future?
                qCDebug(KLEOPATRA_LOG) << "Unhandled keyslot";
            }
        }
    }
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
