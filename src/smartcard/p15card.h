/*  smartcard/p15card.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Andre Heinecke <aheinecke@g10code.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "card.h"

namespace Kleo
{
namespace SmartCard
{
/** Class to work with PKCS#15 smartcards or compatible tokens.
 *
 * A PKCS#15 card is pretty generic and there is no real standard
 * for them. It all depends on the Apps running on the cards. This
 * mostly tries to leave it to GnuPG to determine if there are usable
 * things on the card. The generic info on all keys on the card is
 * accessible through keyInfo from the parent class.
 *
 * The specialization is required for specific app support.
 **/
class P15Card: public Card
{
public:
    explicit P15Card(const Card &card);

    static const std::string AppName;

    void setAppKeyRef(const std::string &appKeyRef,
                      const std::string &value);
    std::string appKeyRef(const std::string &appKeyRef) const;

    /* Obtain an application specific fingerprint for a key
     * stored on this card.
     * e.g. An App Key Ref would be
     *      OpenPGPCard::pgpSigKeyRef */
    std::string appKeyFingerprint(const std::string &appKeyRef) const;

    void setCardInfo(const std::vector< std::pair<std::string, std::string> > &infos);
    void setManufacturer(const std::string &manufacturer);
    std::string manufacturer() const;

    bool operator == (const Card& other) const override;

private:
    std::string mManufacturer;
};
} // namespace Smartcard
} // namespace Kleopatra

