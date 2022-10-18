/*  smartcard/openpgpcard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2020, 2022 g10 Code GmbH
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

#include "algorithminfo.h"

#include <Libkleo/Algorithm>

#include <KLocalizedString>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::SmartCard;

static QDebug operator<<(QDebug s, const std::string &string)
{
    return s << QString::fromStdString(string);
}

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

void OpenPGPCard::setSupportedAlgorithms(const std::vector<std::string> &algorithms)
{
    static const std::vector<std::string> allowedAlgorithms = {
        "brainpoolP256r1",
        "brainpoolP384r1",
        "brainpoolP512r1",
        "curve25519",
        "nistp256",
        "nistp384",
        "nistp521",
        "rsa2048",
        "rsa3072",
        "rsa4096",
    };
    mAlgorithms.clear();
    std::copy_if(algorithms.begin(), algorithms.end(), std::back_inserter(mAlgorithms), [](const auto &algo) {
        return Kleo::contains(allowedAlgorithms, algo);
    });
    if (mAlgorithms.size() < algorithms.size()) {
        qWarning(KLEOPATRA_LOG).nospace() << "OpenPGPCard::" << __func__ << " Invalid algorithm in " << algorithms
                                          << " (allowed algorithms: " << allowedAlgorithms << ")";
    }
}

std::string OpenPGPCard::pubkeyUrl() const
{
    return cardInfo("PUBKEY-URL");
}

std::vector<AlgorithmInfo> OpenPGPCard::supportedAlgorithms(const std::string &keyRef)
{
    static const std::map<std::string, QString> displayNames = {
        { "brainpoolP256r1", i18nc("@info", "ECC (Brainpool P-256)") },
        { "brainpoolP384r1", i18nc("@info", "ECC (Brainpool P-384)") },
        { "brainpoolP512r1", i18nc("@info", "ECC (Brainpool P-512)") },
        { "curve25519", i18nc("@info", "ECC (Curve25519)") },
        { "nistp256", i18nc("@info", "ECC (NIST P-256)") },
        { "nistp384", i18nc("@info", "ECC (NIST P-384)") },
        { "nistp521", i18nc("@info", "ECC (NIST P-521)") },
        { "rsa2048", i18nc("@info", "RSA 2048") },
        { "rsa3072", i18nc("@info", "RSA 3072") },
        { "rsa4096", i18nc("@info", "RSA 4096") },
    };
    const std::string curve25519Algo = keyRef == OpenPGPCard::pgpEncKeyRef() ? "cv25519" : "ed25519";
    std::vector<AlgorithmInfo> algos;
    std::transform(mAlgorithms.cbegin(), mAlgorithms.cend(), std::back_inserter(algos), [curve25519Algo](const auto &algo) {
        if (algo == "curve25519") {
            return AlgorithmInfo{curve25519Algo, displayNames.at(algo)};
        }
        return AlgorithmInfo{algo, displayNames.at(algo)};
    });
    return algos;
}
