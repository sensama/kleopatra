/*  smartcard/pivcard.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef SMARTCARD_PIVCARD_H
#define SMARTCARD_PIVCARD_H

#include <QMap>

#include "card.h"

namespace Kleo
{
namespace SmartCard
{
/** Class to work with PIV smartcards or compatible tokens */
class PIVCard: public Card
{
public:
    PIVCard ();
    PIVCard (const std::string &serialno);

    std::string pivAuthenticationKeyGrip() const;
    std::string cardAuthenticationKeyGrip() const;
    std::string digitalSignatureKeyGrip() const;
    std::string keyManagementKeyGrip() const;

    void setCardInfo (const std::vector< std::pair<std::string, std::string> > &infos);

    std::string displaySerialNumber() const;
    void setDisplaySerialNumber(const std::string &sn);

    bool operator == (const Card& other) const override;

private:
    std::string mDisplaySerialNumber;
    QMap <std::string, std::string> mMetaInfo;
};
} // namespace Smartcard
} // namespace Kleopatra

#endif // SMARTCARD_PIVCARD_H
