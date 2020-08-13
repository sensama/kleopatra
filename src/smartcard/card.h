#ifndef SMARTCARD_CARD_H
#define SMARTCARD_CARD_H
/*  smartcard/card.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <vector>
#include <string>

#include <QString>

namespace Kleo
{
namespace SmartCard
{
/** Class to work with Smartcards or other Hardware tokens. */
class Card
{
public:
    enum AppType {
        UnknownApplication,
        OpenPGPApplication,
        NksApplication,
        P15Application,
        DinSigApplication,
        GeldkarteApplication,

        NumAppTypes
    };

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
    virtual ~Card() {}

    virtual bool operator == (const Card& other) const;
    bool operator != (const Card& other) const;

    void setStatus(Status s);
    Status status() const;

    virtual void setSerialNumber(const std::string &sn);
    std::string serialNumber() const;

    AppType appType() const;
    void setAppType(AppType type);

    void setAppVersion(int version);
    int appVersion() const;

    std::vector<PinState> pinStates() const;
    void setPinStates(const std::vector<PinState> &pinStates);

    void setSlot(int slot);
    int slot() const;

    bool hasNullPin() const;
    void setHasNullPin(bool value);

    bool canLearnKeys() const;
    void setCanLearnKeys(bool value);

    QString errorMsg() const;
    void setErrorMsg(const QString &msg);

private:
    bool mCanLearn;
    bool mHasNullPin;
    Status mStatus;
    std::string mSerialNumber;
    AppType mAppType;
    int mAppVersion;
    std::vector<PinState> mPinStates;
    int mSlot;
    QString mErrMsg;
};
} // namespace Smartcard
} // namespace Kleopatra

#endif // SMARTCARD_CARD_H
