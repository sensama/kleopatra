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

    static std::string pivAuthenticationKeyRef();
    static std::string cardAuthenticationKeyRef();
    static std::string digitalSignatureKeyRef();
    static std::string keyManagementKeyRef();

    static std::string pinKeyRef();
    static std::string pukKeyRef();

    static const std::vector<std::string> & supportedKeys();
    static QString keyDisplayName(const std::string &keyRef);
    static std::vector< std::pair<std::string, QString> > supportedAlgorithms(const std::string &keyRef);

    std::string keyGrip(const std::string &keyRef) const;

    void setCardInfo (const std::vector< std::pair<std::string, std::string> > &infos);

    std::string displaySerialNumber() const;
    void setDisplaySerialNumber(const std::string &sn);

    std::string keyAlgorithm(const std::string &keyRef) const;
    void setKeyAlgorithm(const std::string &keyRef, const std::string &algorithm);

    bool operator == (const Card& other) const override;

private:
    std::string mDisplaySerialNumber;
    QMap <std::string, std::string> mMetaInfo;
};
} // namespace Smartcard
} // namespace Kleopatra

#endif // SMARTCARD_PIVCARD_H
