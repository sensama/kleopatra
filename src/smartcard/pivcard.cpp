/*  smartcard/pivcard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pivcard.h"

#include "keypairinfo.h"

#include <KLocalizedString>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::SmartCard;

// static
const std::string PIVCard::AppName = "piv";

PIVCard::PIVCard(const Card &card)
    : Card(card)
{
    setAppName(AppName);
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
const std::vector<std::string> & PIVCard::supportedKeys()
{
    static const std::vector<std::string> keyRefs = {
        PIVCard::pivAuthenticationKeyRef(),
        PIVCard::cardAuthenticationKeyRef(),
        PIVCard::digitalSignatureKeyRef(),
        PIVCard::keyManagementKeyRef()
    };

    return keyRefs;
}

// static
QString PIVCard::keyDisplayName(const std::string &keyRef)
{
    static const QMap<std::string, QString> displayNames = {
        { PIVCard::pivAuthenticationKeyRef(), i18n("PIV Authentication Key") },
        { PIVCard::cardAuthenticationKeyRef(), i18n("Card Authentication Key") },
        { PIVCard::digitalSignatureKeyRef(), i18n("Digital Signature Key") },
        { PIVCard::keyManagementKeyRef(), i18n("Key Management Key") },
    };

    return displayNames.value(keyRef);
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

void PIVCard::setCardInfo(const std::vector< std::pair<std::string, std::string> > &infos)
{
    qCDebug(KLEOPATRA_LOG) << "Card" << serialNumber().c_str() << "info:";
    for (const auto &pair: infos) {
        qCDebug(KLEOPATRA_LOG) << pair.first.c_str() << ":" << pair.second.c_str();
        if (parseCardInfo(pair.first, pair.second)) {
            continue;
        }
        if (pair.first == "KEYPAIRINFO") {
            const KeyPairInfo info = KeyPairInfo::fromStatusLine(pair.second);
            if (info.grip.empty()) {
                qCWarning(KLEOPATRA_LOG) << "Invalid KEYPAIRINFO status line"
                        << QString::fromStdString(pair.second);
                setStatus(Card::CardError);
                continue;
            }
            mMetaInfo.insert("KEYPAIRINFO-" + info.keyRef, info.grip);
        } else {
            mMetaInfo.insert(pair.first, pair.second);
        }
    }
}

std::string PIVCard::keyAlgorithm(const std::string &keyRef) const
{
    return mMetaInfo.value("KLEO-KEYALGO-" + keyRef);
}

void PIVCard::setKeyAlgorithm(const std::string &keyRef, const std::string &algorithm)
{
    mMetaInfo.insert("KLEO-KEYALGO-" + keyRef, algorithm);
}

std::string PIVCard::certificateData(const std::string &keyRef) const
{
    return mMetaInfo.value("KLEO-CERTIFICATE-" + keyRef);
}

void PIVCard::setCertificateData(const std::string &keyRef, const std::string &data)
{
    mMetaInfo.insert("KLEO-CERTIFICATE-" + keyRef, data);
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
