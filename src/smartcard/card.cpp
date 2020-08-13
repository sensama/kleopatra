/*  smartcard/card.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "card.h"
#include "readerstatus.h"

using namespace Kleo;
using namespace Kleo::SmartCard;

Card::Card(): mCanLearn(false),
              mHasNullPin(false),
              mStatus(Status::NoCard),
              mAppType(UnknownApplication),
              mAppVersion(-1) {
}

void Card::setStatus(Status s)
{
    mStatus = s;
}

Card::Status Card::status() const
{
    return mStatus;
}

void Card::setSerialNumber(const std::string &sn)
{
    mSerialNumber = sn;
}

std::string Card::serialNumber() const
{
    return mSerialNumber;
}

Card::AppType Card::appType() const
{
    return mAppType;
}

void Card::setAppType(AppType t)
{
    mAppType = t;
}

void Card::setAppVersion(int version)
{
    mAppVersion = version;
}

int Card::appVersion() const
{
    return mAppVersion;
}

std::vector<Card::PinState> Card::pinStates() const
{
    return mPinStates;
}

void Card::setPinStates(const std::vector<PinState> &pinStates)
{
    mPinStates = pinStates;
}

void Card::setSlot(int slot)
{
    mSlot = slot;
}

int Card::slot() const
{
    return mSlot;
}

bool Card::hasNullPin() const
{
    return mHasNullPin;
}

void Card::setHasNullPin(bool value)
{
    mHasNullPin = value;
}

bool Card::canLearnKeys() const
{
    return mCanLearn;
}

void Card::setCanLearnKeys(bool value)
{
    mCanLearn = value;
}

bool Card::operator == (const Card& other) const
{
    return mStatus == other.status()
        && mSerialNumber == other.serialNumber()
        && mAppType == other.appType()
        && mAppVersion == other.appVersion()
        && mPinStates == other.pinStates()
        && mSlot == other.slot()
        && mCanLearn == other.canLearnKeys()
        && mHasNullPin == other.hasNullPin();
}

bool Card::operator != (const Card& other) const
{
    return !operator==(other);
}

void Card::setErrorMsg(const QString &msg)
{
    mErrMsg = msg;
}

QString Card::errorMsg() const
{
    return mErrMsg;
}
