#ifndef SMARTCARD_OPENPGPCARD_H
#define SMARTCARD_OPENPGPCARD_H
/*  smartcard/openpgpcard.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QMap>

#include "card.h"

namespace Kleo
{
namespace SmartCard
{
/** Class to work with OpenPGP smartcards or compatible tokens */
class OpenPGPCard: public Card
{
public:
    explicit OpenPGPCard(const Card &card);

    static const std::string AppName;

    static std::string pinKeyRef();
    static std::string adminPinKeyRef();
    static std::string resetCodeKeyRef();

    void setSerialNumber(const std::string &sn) override;

    std::string encFpr() const;
    std::string sigFpr() const;
    std::string authFpr() const;

    void setKeyPairInfo (const std::vector< std::pair<std::string, std::string> > &infos);

    bool operator == (const Card& other) const override;

    void setManufacturer(const std::string &manufacturer);
    std::string manufacturer() const;

    std::string cardVersion() const;
    std::string cardHolder() const;
    std::string pubkeyUrl() const;
private:
    bool mIsV2 = false;
    std::string mCardVersion;
    QMap <std::string, std::string> mMetaInfo;
    std::string mManufacturer;
};
} // namespace Smartcard
} // namespace Kleopatra

#endif // SMARTCARD_CARD_H

