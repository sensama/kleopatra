/*  smartcard/pivcard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pivcard.h"

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::SmartCard;

PIVCard::PIVCard()
{
    setAppType(Card::PivApplication);
}

PIVCard::PIVCard(const std::string &serialno): PIVCard()
{
    setSerialNumber(serialno);
}

std::string PIVCard::pivAuthenticationKeyGrip() const
{
    return mMetaInfo.value("PIV-AUTH-KEYPAIRINFO");
}

std::string PIVCard::cardAuthenticationKeyGrip() const
{
    return mMetaInfo.value("CARD-AUTH-KEYPAIRINFO");
}

std::string PIVCard::digitalSignatureKeyGrip() const
{
    return mMetaInfo.value("SIG-KEYPAIRINFO");
}

std::string PIVCard::keyManagementKeyGrip() const
{
    return mMetaInfo.value("ENC-KEYPAIRINFO");
}

namespace {
static int parseAppVersion(const std::string &s) {
    // s is a hex-encoded, unsigned int-packed version tuple
    bool ok;
    const auto appVersion = QByteArray::fromStdString(s).toUInt(&ok, 16);
    return ok ? appVersion : -1;
}
}

void PIVCard::setCardInfo(const std::vector< std::pair<std::string, std::string> > &infos)
{
    qCDebug(KLEOPATRA_LOG) << "Card" << serialNumber().c_str() << "info:";
    for (const auto &pair: infos) {
        qCDebug(KLEOPATRA_LOG) << pair.first.c_str() << ":" << pair.second.c_str();
        if (pair.first == "APPVERSION") {
            setAppVersion(parseAppVersion(pair.second));
        } else if (pair.first == "KEYPAIRINFO") {
            const auto values = QString::fromStdString(pair.second).split(QLatin1Char(' '));
            if (values.size() != 3) {
                qCWarning(KLEOPATRA_LOG) << "Invalid KEYPAIRINFO entry" << QString::fromStdString(pair.second);
                setStatus(Card::CardError);
                continue;
            }
            const auto grip = values[0].toStdString();
            const auto slotId = values[1];
            const auto usage = values[2];
            if (slotId == QLatin1String("PIV.9A")) {
                mMetaInfo.insert(std::string("PIV-AUTH-") + pair.first, grip);
            } else if (slotId == QLatin1String("PIV.9E")) {
                mMetaInfo.insert(std::string("CARD-AUTH-") + pair.first, grip);
            } else if (slotId == QLatin1String("PIV.9C")) {
                mMetaInfo.insert(std::string("SIG-") + pair.first, grip);
            } else if (slotId == QLatin1String("PIV.9D")) {
                mMetaInfo.insert(std::string("ENC-") + pair.first, grip);
            } else {
                qCDebug(KLEOPATRA_LOG) << "Unhandled keyslot";
            }
        } else {
            mMetaInfo.insert(pair.first, pair.second);
        }
    }
}

std::string PIVCard::displaySerialNumber() const
{
    return mDisplaySerialNumber;
}

void PIVCard::setDisplaySerialNumber(const std::string &serialno)
{
    mDisplaySerialNumber = serialno;
}

bool PIVCard::operator == (const Card& rhs) const
{
    const PIVCard *other = dynamic_cast<const PIVCard *>(&rhs);
    if (!other) {
        return false;
    }

    return Card::operator ==(rhs)
        && pivAuthenticationKeyGrip() == other->pivAuthenticationKeyGrip()
        && cardAuthenticationKeyGrip() == other->cardAuthenticationKeyGrip()
        && digitalSignatureKeyGrip() == other->digitalSignatureKeyGrip()
        && keyManagementKeyGrip() == other->keyManagementKeyGrip();
}
