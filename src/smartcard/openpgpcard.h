/*  smartcard/openpgpcard.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2020, 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "card.h"

namespace Kleo
{
namespace SmartCard
{
struct AlgorithmInfo;
struct KeyPairInfo;

/** Class to work with OpenPGP smartcards or compatible tokens */
class OpenPGPCard: public Card
{
public:
    explicit OpenPGPCard(const Card &card);

    static const std::string AppName;

    static std::string pgpSigKeyRef();
    static std::string pgpEncKeyRef();
    static std::string pgpAuthKeyRef();

    static std::string pinKeyRef();
    static std::string adminPinKeyRef();
    static std::string resetCodeKeyRef();

    static const std::vector<KeyPairInfo> & supportedKeys();
    static QString keyDisplayName(const std::string &keyRef);

    /**
     * Sets the algorithms supported by this smart card to \p algorithms.
     * The following values for algorithms are allowed:
     *   brainpoolP256r1, brainpoolP384r1, brainpoolP512r1,
     *   curve25519,
     *   nistp256, nistp384, nistp521,
     *   rsa2048, rsa3072, rsa4096.
     */
    void setSupportedAlgorithms(const std::vector<std::string> &algorithms);

    std::string pubkeyUrl() const;

    /**
     * Returns a list of algorithm names and corresponding display names suitable
     * for the card slot specified by \p keyRef.
     *
     * \note For Curve25519, depending on the given card slot, either "ed25519"
     *       or "cv25519" is returned as algorithm ID.
     */
    std::vector<AlgorithmInfo> supportedAlgorithms(const std::string &keyRef);

private:
    std::vector<std::string> mAlgorithms;
};
} // namespace Smartcard
} // namespace Kleopatra


