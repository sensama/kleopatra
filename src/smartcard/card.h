#ifndef SMARTCARD_CARD_H
#define SMARTCARD_CARD_H
/*  smartcard/card.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

    std::string appName() const;

    void setAppVersion(int version);
    int appVersion() const;
    QString displayAppVersion() const;

    std::string cardType() const;

    int cardVersion() const;
    QString displayCardVersion() const;

    std::vector<PinState> pinStates() const;
    void setPinStates(const std::vector<PinState> &pinStates);

    bool hasNullPin() const;
    void setHasNullPin(bool value);

    bool canLearnKeys() const;
    void setCanLearnKeys(bool value);

    QString errorMsg() const;
    void setErrorMsg(const QString &msg);

protected:
    void setAppName(const std::string &name);

    bool parseCardInfo(const std::string &name, const std::string &value);

private:
    bool mCanLearn = false;
    bool mHasNullPin = false;
    Status mStatus = NoCard;
    std::string mSerialNumber;
    std::string mAppName;
    int mAppVersion = -1;
    std::string mCardType;
    int mCardVersion = -1;
    std::vector<PinState> mPinStates;
    QString mErrMsg;
};
} // namespace Smartcard
} // namespace Kleopatra

#endif // SMARTCARD_CARD_H
