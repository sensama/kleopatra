#ifndef SMARTCARD_CARD_H
#define SMARTCARD_CARD_H
/*  smartcard/card.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "keypairinfo.h"

#include <string>
#include <vector>

#include <QString>

namespace Kleo
{
namespace SmartCard
{

/** Class representing an application on a smartcard or similar hardware token. */
class Card
{
public:
    enum PinState {
        UnknownPinState,
        NullPin,
        PinBlocked,
        NoPin,
        PinOk,

        NumPinStates
    };

    enum Status {
        NoCard,
        CardPresent,
        CardActive,
        CardUsable,

        _NumScdStates,

        CardError = _NumScdStates,

        NumStates
    };

    Card();
    virtual ~Card();

    virtual bool operator == (const Card &other) const;
    bool operator != (const Card &other) const;

    void setStatus(Status s);
    Status status() const;

    void setSerialNumber(const std::string &sn);
    std::string serialNumber() const;

    QString displaySerialNumber() const;
    void setDisplaySerialNumber(const QString &sn);

    std::string appName() const;

    void setAppVersion(int version);
    int appVersion() const;
    QString displayAppVersion() const;

    std::string cardType() const;

    int cardVersion() const;
    QString displayCardVersion() const;

    QString cardHolder() const;

    std::vector<PinState> pinStates() const;
    void setPinStates(const std::vector<PinState> &pinStates);

    bool hasNullPin() const;
    void setHasNullPin(bool value);

    bool canLearnKeys() const;
    void setCanLearnKeys(bool value);

    QString errorMsg() const;
    void setErrorMsg(const QString &msg);

    const std::vector<KeyPairInfo> & keyInfos() const;
    const KeyPairInfo & keyInfo(const std::string &keyRef) const;

protected:
    void setAppName(const std::string &name);

    bool parseCardInfo(const std::string &name, const std::string &value);

private:
    void updateKeyInfo(const KeyPairInfo &keyPairInfo);

private:
    bool mCanLearn = false;
    bool mHasNullPin = false;
    Status mStatus = NoCard;
    std::string mSerialNumber;
    QString mDisplaySerialNumber;
    std::string mAppName;
    int mAppVersion = -1;
    std::string mCardType;
    int mCardVersion = -1;
    QString mCardHolder;
    std::vector<PinState> mPinStates;
    QString mErrMsg;
    std::vector<KeyPairInfo> mKeyInfos;
};
} // namespace Smartcard
} // namespace Kleopatra

#endif // SMARTCARD_CARD_H
