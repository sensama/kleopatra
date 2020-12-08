#ifndef SMARTCARD_NETKEYCARD_H
#define SMARTCARD_NETKEYCARD_H
/*  smartcard/netkeycard.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "card.h"

#include <gpgme++/key.h>

namespace Kleo
{
namespace SmartCard
{
struct KeyPairInfo;

/** Class to work with NetKey smartcards or compatible tokens */
class NetKeyCard: public Card
{
public:
    explicit NetKeyCard(const Card &card);

    static const std::string AppName;

    void setCardInfo(const std::vector< std::pair<std::string, std::string> > &infos);

    bool hasSigGNullPin() const;
    bool hasNKSNullPin() const;

    std::vector <GpgME::Key> keys() const;

private:
    void setKeyPairInfo(const std::vector<KeyPairInfo> &infos);

private:
    std::vector <GpgME::Key> mKeys;
};
} // namespace Smartcard
} // namespace Kleopatra

#endif // SMARTCARD_CARD_H

