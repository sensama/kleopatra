/*  smartcard/openpgpcard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/* Code in this file is partly based on the GNU Privacy Assistant
 * (cm-openpgp.c) git rev. 0a78795146661234070681737b3e08228616441f
 *
 * Whis is:
 * SPDX-FileCopyrightText: 2008, 2009 g 10 Code GmbH
 *
 * And may be licensed under the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "openpgpcard.h"

#include "kleopatra_debug.h"


using namespace Kleo;
using namespace Kleo::SmartCard;

// static
const std::string OpenPGPCard::AppName = "openpgp";

OpenPGPCard::OpenPGPCard(const Card &card)
    : Card(card)
{
    setAppName(AppName);
}

// static
std::string OpenPGPCard::pinKeyRef()
{
    return std::string("OPENPGP.1");
}

// static
std::string OpenPGPCard::adminPinKeyRef()
{
    return std::string("OPENPGP.3");
}

// static
std::string OpenPGPCard::resetCodeKeyRef()
{
    return std::string("OPENPGP.2");
}

std::string OpenPGPCard::sigFpr() const
{
    return mMetaInfo.value("SIGKEY-FPR");
}

std::string OpenPGPCard::encFpr() const
{
    return mMetaInfo.value("ENCKEY-FPR");
}

std::string OpenPGPCard::authFpr() const
{
    return mMetaInfo.value("AUTHKEY-FPR");
}

void OpenPGPCard::setCardInfo(const std::vector< std::pair<std::string, std::string> > &infos)
{
    qCDebug(KLEOPATRA_LOG) << "Card" << serialNumber().c_str() << "info:";
    for (const auto &pair: infos) {
        qCDebug(KLEOPATRA_LOG) << pair.first.c_str() << ":" << pair.second.c_str();
        if (parseCardInfo(pair.first, pair.second)) {
            continue;
        }
        if (pair.first == "KEY-FPR" ||
            pair.first == "KEY-TIME") {
            // Key fpr and key time need to be distinguished, the number
            // of the key decides the usage.
            const auto values = QString::fromStdString(pair.second).split(QLatin1Char(' '));
            if (values.size() < 2) {
                qCWarning(KLEOPATRA_LOG) << "Invalid entry.";
                setStatus(Card::CardError);
                continue;
            }
            const auto usage = values[0];
            const auto fpr = values[1].toStdString();
            if (usage == QLatin1Char('1')) {
                mMetaInfo.insert(std::string("SIG") + pair.first, fpr);
            } else if (usage == QLatin1Char('2')) {
                mMetaInfo.insert(std::string("ENC") + pair.first, fpr);
            } else if (usage == QLatin1Char('3')) {
                mMetaInfo.insert(std::string("AUTH") + pair.first, fpr);
            } else {
                // Maybe more keyslots in the future?
                qCDebug(KLEOPATRA_LOG) << "Unhandled keyslot";
            }
        } else if (pair.first == "KEYPAIRINFO") {
            // Fun, same as above but the other way around.
            const auto values = QString::fromStdString(pair.second).split(QLatin1Char(' '));
            if (values.size() < 2) {
                qCWarning(KLEOPATRA_LOG) << "Invalid entry.";
                setStatus(Card::CardError);
                continue;
            }
            const auto usage = values[1];
            const auto grip = values[0].toStdString();
            if (usage == QLatin1String("OPENPGP.1")) {
                mMetaInfo.insert(std::string("SIG") + pair.first, grip);
            } else if (usage == QLatin1String("OPENPGP.2")) {
                mMetaInfo.insert(std::string("ENC") + pair.first, grip);
            } else if (usage == QLatin1String("OPENPGP.3")) {
                mMetaInfo.insert(std::string("AUTH") + pair.first, grip);
            } else {
                // Maybe more keyslots in the future?
                qCDebug(KLEOPATRA_LOG) << "Unhandled keyslot";
            }
        } else {
            mMetaInfo.insert(pair.first, pair.second);
        }
    }
}

bool OpenPGPCard::operator == (const Card& rhs) const
{
    const OpenPGPCard *other = dynamic_cast<const OpenPGPCard *>(&rhs);
    if (!other) {
        return false;
    }

    return Card::operator ==(rhs)
        && sigFpr() == other->sigFpr()
        && encFpr() == other->encFpr()
        && authFpr() == other->authFpr()
        && manufacturer() == other->manufacturer()
        && cardHolder() == other->cardHolder()
        && pubkeyUrl() == other->pubkeyUrl();
}

void OpenPGPCard::setManufacturer(const std::string &manufacturer)
{
    mManufacturer = manufacturer;
}

std::string OpenPGPCard::manufacturer() const
{
    return mManufacturer;
}

std::string OpenPGPCard::pubkeyUrl() const
{
    return mMetaInfo.value("PUBKEY-URL");
}
