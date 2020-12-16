/*  smartcard/openpgpcard.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef SMARTCARD_OPENPGPCARD_H
#define SMARTCARD_OPENPGPCARD_H

#include "card.h"

#include <QMap>

namespace Kleo
{
namespace SmartCard
{
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

    void setCardInfo(const std::vector< std::pair<std::string, std::string> > &infos);

    std::string keyFingerprint(const std::string &keyRef) const;

    bool operator == (const Card& other) const override;

    void setManufacturer(const std::string &manufacturer);
    std::string manufacturer() const;

    std::string pubkeyUrl() const;

private:
    QMap <std::string, std::string> mMetaInfo;
    std::string mManufacturer;
};
} // namespace Smartcard
} // namespace Kleopatra

#endif // SMARTCARD_CARD_H

