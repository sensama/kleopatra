/*  smartcard/openpgpcard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

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

#include <KLocalizedString>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::SmartCard;

// static
const std::string OpenPGPCard::AppName = "openpgp";

OpenPGPCard::OpenPGPCard(const Card &card)
    : Card(card)
{
    setAppName(AppName);
    setInitialKeyInfos(OpenPGPCard::supportedKeys());
}

// static
std::string OpenPGPCard::pgpSigKeyRef()
{
    return std::string("OPENPGP.1");
}

// static
std::string OpenPGPCard::pgpEncKeyRef()
{
    return std::string("OPENPGP.2");
}

// static
std::string OpenPGPCard::pgpAuthKeyRef()
{
    return std::string("OPENPGP.3");
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

// static
const std::vector<KeyPairInfo> & OpenPGPCard::supportedKeys()
{
    static const std::vector<KeyPairInfo> keyInfos = {
        {OpenPGPCard::pgpSigKeyRef(), "", "sc", "", ""},
        {OpenPGPCard::pgpEncKeyRef(), "", "e", "", ""},
        {OpenPGPCard::pgpAuthKeyRef(), "", "a", "", ""}
    };

    return keyInfos;
}

// static
QString OpenPGPCard::keyDisplayName(const std::string &keyRef)
{
    static const QMap<std::string, QString> displayNames = {
        { OpenPGPCard::pgpSigKeyRef(), i18n("Signature") },
        { OpenPGPCard::pgpEncKeyRef(), i18n("Encryption") },
        { OpenPGPCard::pgpAuthKeyRef(), i18n("Authentication") }
    };

    return displayNames.value(keyRef);
}

void OpenPGPCard::setCardInfo(const std::vector< std::pair<std::string, std::string> > &infos)
{
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

std::string OpenPGPCard::keyFingerprint(const std::string &keyRef) const
{
    return cardInfo("KLEO-FPR-" + keyRef);
}

bool OpenPGPCard::operator == (const Card& rhs) const
{
    const auto other = dynamic_cast<const OpenPGPCard *>(&rhs);
    if (!other) {
        return false;
    }

    return Card::operator ==(rhs)
        && mManufacturer == other->mManufacturer;
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
    return cardInfo("PUBKEY-URL");
}
