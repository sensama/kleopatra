/*  smartcard/pivcard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pivcard.h"

#include <KLocalizedString>

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

// static
std::string PIVCard::pivAuthenticationKeyRef()
{
    return std::string("PIV.9A");
}

// static
std::string PIVCard::cardAuthenticationKeyRef()
{
    return std::string("PIV.9E");
}

// static
std::string PIVCard::digitalSignatureKeyRef()
{
    return std::string("PIV.9C");
}

// static
std::string PIVCard::keyManagementKeyRef()
{
    return std::string("PIV.9D");
}

// static
std::string PIVCard::pinKeyRef()
{
    return std::string("PIV.80");
}

// static
std::string PIVCard::pukKeyRef()
{
    return std::string("PIV.81");
}

// static
std::vector< std::pair<std::string, QString> > PIVCard::supportedAlgorithms(const std::string &keyRef)
{
    if (keyRef == PIVCard::keyManagementKeyRef()) {
        return {
            { "rsa2048", i18n("RSA key transport (2048 bits)") },
            { "nistp256", i18n("ECDH (Curve P-256)") },
            { "nistp384", i18n("ECDH (Curve P-384)") }
        };
    } else if (keyRef == PIVCard::digitalSignatureKeyRef()) {
        return {
            { "rsa2048", i18n("RSA (2048 bits)") },
            { "nistp256", i18n("ECDSA (Curve P-256)") },
            { "nistp384", i18n("ECDSA (Curve P-384)") }
        };
    }

    // NIST SP 800-78-4 does not allow Curve P-384 for PIV Authentication key or Card Authentication key
    return {
        { "rsa2048", i18n("RSA (2048 bits)") },
        { "nistp256", i18n("ECDSA (Curve P-256)") },
    };
}

std::string PIVCard::keyGrip(const std::string& keyRef) const
{
    return mMetaInfo.value("KEYPAIRINFO-" + keyRef);
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
            const auto keyRef = values[1].toStdString();
            //const auto usage = values[2];
            mMetaInfo.insert("KEYPAIRINFO-" + keyRef, grip);
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
        && mMetaInfo == other->mMetaInfo;
}
