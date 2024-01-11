/*  smartcard/netkeycard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "netkeycard.h"

#include "keypairinfo.h"

#include "kleopatra_debug.h"

#include <Libkleo/Algorithm>
#include <Libkleo/Formatting>
#include <Libkleo/Predicates>

#include <gpgme++/context.h>
#include <gpgme++/error.h>
#include <gpgme++/keylistresult.h>

#include <memory>
#include <string>

using namespace Kleo;
using namespace Kleo::SmartCard;

// static
const std::string NetKeyCard::AppName = "nks";

namespace
{
static GpgME::Key lookup_key(GpgME::Context *ctx, const std::string &keyGrip)
{
    if (!ctx || keyGrip.empty()) {
        return GpgME::Key();
    }
    const std::string pattern = '&' + keyGrip;
    qCDebug(KLEOPATRA_LOG) << "parse_keypairinfo_and_lookup_key: pattern=" << pattern.c_str();
    if (const auto err = ctx->startKeyListing(pattern.c_str())) {
        qCDebug(KLEOPATRA_LOG) << "parse_keypairinfo_and_lookup_key: startKeyListing failed:" << Formatting::errorAsString(err);
        return GpgME::Key();
    }
    GpgME::Error e;
    const auto key = ctx->nextKey(e);
    ctx->endKeyListing();
    qCDebug(KLEOPATRA_LOG) << "parse_keypairinfo_and_lookup_key: e=" << e.code() << "; key.isNull()" << key.isNull();
    return key;
}

} // namespace

NetKeyCard::NetKeyCard(const Card &card)
    : Card(card)
{
    setAppName(AppName);
}

// static
std::string NetKeyCard::nksPinKeyRef()
{
    return std::string("PW1.CH");
}

// static
std::string NetKeyCard::sigGPinKeyRef()
{
    return std::string("PW1.CH.SIG");
}

void NetKeyCard::processCardInfo()
{
    setKeyPairInfo(keyInfos());
}

void NetKeyCard::setKeyPairInfo(const std::vector<KeyPairInfo> &infos)
{
    // check that any of the keys are new
    const std::unique_ptr<GpgME::Context> klc(GpgME::Context::createForProtocol(GpgME::CMS));
    if (!klc.get()) {
        return;
    }
    klc->setKeyListMode(GpgME::Ephemeral);
    klc->addKeyListMode(GpgME::Validate);

    setCanLearnKeys(false);
    mKeys.clear();
    for (const auto &info : infos) {
        const auto key = lookup_key(klc.get(), info.grip);
        if (key.isNull()) {
            setCanLearnKeys(true);
        } else {
            mKeys.push_back(key);
        }
    }
}

// State 0 -> NKS PIN Retry counter
// State 1 -> NKS PUK Retry counter
// State 2 -> SigG PIN Retry counter
// State 3 -> SigG PUK Retry counter

bool NetKeyCard::hasNKSNullPin() const
{
    const auto states = pinStates();
    if (states.size() < 2) {
        qCWarning(KLEOPATRA_LOG) << "Invalid size of pin states:" << states.size();
        return false;
    }
    return states[0] == Card::NullPin;
}

bool NetKeyCard::hasSigGNullPin() const
{
    const auto states = pinStates();
    if (states.size() < 4) {
        qCWarning(KLEOPATRA_LOG) << "Invalid size of pin states:" << states.size();
        return false;
    }
    return states[2] == Card::NullPin;
}

std::vector<GpgME::Key> NetKeyCard::keys() const
{
    return mKeys;
}

bool NetKeyCard::operator==(const Card &other) const
{
    static const _detail::ByFingerprint<std::equal_to> keysHaveSameFingerprint;

    if (!Card::operator==(other)) {
        qCDebug(KLEOPATRA_LOG) << "NetKeyCard" << __func__ << "Card don't match";
        return false;
    }

    const auto otherNetKeyCard = dynamic_cast<const NetKeyCard *>(&other);
    if (!otherNetKeyCard) {
        qCWarning(KLEOPATRA_LOG) << "Failed to cast other card to NetKeyCard";
        return false;
    }
    if (mKeys.size() != otherNetKeyCard->mKeys.size()) {
        qCDebug(KLEOPATRA_LOG) << "NetKeyCard" << __func__ << "Number of keys doesn't match";
        return false;
    }
    const auto otherHasKey = [otherNetKeyCard](const GpgME::Key &key) {
        return Kleo::any_of(otherNetKeyCard->mKeys, [key](const GpgME::Key &otherKey) {
            return keysHaveSameFingerprint(key, otherKey);
        });
    };
    const bool result = Kleo::all_of(mKeys, otherHasKey);
    qCDebug(KLEOPATRA_LOG) << "NetKeyCard" << __func__ << "Keys match?" << result;
    return result;
}
