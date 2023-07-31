/* -*- mode: c++; c-basic-offset:4 -*-
    utils/keys.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "keys.h"

#include <kleopatra_debug.h>
#include <settings.h>

#include <Libkleo/Algorithm>
#include <Libkleo/KeyCache>

#include <QDate>

// needed for GPGME_VERSION_NUMBER
#include <gpgme.h>

#include <algorithm>
#include <iterator>

namespace
{
bool isLastValidUserID(const GpgME::UserID &userId)
{
    if (Kleo::isRevokedOrExpired(userId)) {
        return false;
    }
    const auto userIds = userId.parent().userIDs();
    const int numberOfValidUserIds = std::count_if(std::begin(userIds), std::end(userIds), [](const auto &u) {
        return !Kleo::isRevokedOrExpired(u);
    });
    return numberOfValidUserIds == 1;
}

bool hasValidUserID(const GpgME::Key &key)
{
    return Kleo::any_of(key.userIDs(), [](const auto &u) {
        return !Kleo::isRevokedOrExpired(u);
    });
}
}

bool Kleo::isSelfSignature(const GpgME::UserID::Signature &signature)
{
    return !qstrcmp(signature.parent().parent().keyID(), signature.signerKeyID());
}

bool Kleo::isRevokedOrExpired(const GpgME::UserID &userId)
{
    if (userId.isRevoked() || userId.parent().isExpired()) {
        return true;
    }
    const auto sigs = userId.signatures();
    std::vector<GpgME::UserID::Signature> selfSigs;
    std::copy_if(std::begin(sigs), std::end(sigs), std::back_inserter(selfSigs), &Kleo::isSelfSignature);
    std::sort(std::begin(selfSigs), std::end(selfSigs));
    // check the most recent signature
    const auto sig = !selfSigs.empty() ? selfSigs.back() : GpgME::UserID::Signature{};
    return !sig.isNull() && (sig.isRevokation() || sig.isExpired());
}

bool Kleo::canCreateCertifications(const GpgME::Key &key)
{
    return key.canCertify() && canBeUsedForSecretKeyOperations(key);
}

bool Kleo::canBeCertified(const GpgME::Key &key)
{
    return key.protocol() == GpgME::OpenPGP //
        && !key.isBad() //
        && hasValidUserID(key);
}

bool Kleo::canBeUsedForSecretKeyOperations(const GpgME::Key &key)
{
#if GPGME_VERSION_NUMBER >= 0x011102 // 1.17.2
    // we need to check the primary subkey because Key::hasSecret() is also true if just the secret key stub of an offline key is available
    return key.subkey(0).isSecret();
#else
    // older versions of GpgME did not always set the secret flag for card keys
    return key.subkey(0).isSecret() || key.subkey(0).isCardKey();
#endif
}

bool Kleo::canRevokeUserID(const GpgME::UserID &userId)
{
    return (!userId.isNull() //
            && userId.parent().protocol() == GpgME::OpenPGP //
            && !isLastValidUserID(userId));
}

bool Kleo::isSecretKeyStoredInKeyRing(const GpgME::Key &key)
{
    return key.subkey(0).isSecret() && !key.subkey(0).isCardKey();
}

bool Kleo::userHasCertificationKey()
{
    const auto secretKeys = KeyCache::instance()->secretKeys();
    return Kleo::any_of(secretKeys, [](const auto &k) {
        return (k.protocol() == GpgME::OpenPGP) && canCreateCertifications(k);
    });
}

Kleo::CertificationRevocationFeasibility Kleo::userCanRevokeCertification(const GpgME::UserID::Signature &certification)
{
    const auto certificationKey = KeyCache::instance()->findByKeyIDOrFingerprint(certification.signerKeyID());
    const bool isSelfSignature = qstrcmp(certification.parent().parent().keyID(), certification.signerKeyID()) == 0;
    if (!certificationKey.hasSecret()) {
        return CertificationNotMadeWithOwnKey;
    } else if (isSelfSignature) {
        return CertificationIsSelfSignature;
    } else if (certification.isRevokation()) {
        return CertificationIsRevocation;
    } else if (certification.isExpired()) {
        return CertificationIsExpired;
    } else if (certification.isInvalid()) {
        return CertificationIsInvalid;
    } else if (!canCreateCertifications(certificationKey)) {
        return CertificationKeyNotAvailable;
    }
    return CertificationCanBeRevoked;
}

bool Kleo::userCanRevokeCertifications(const GpgME::UserID &userId)
{
    if (userId.numSignatures() == 0) {
        qCWarning(KLEOPATRA_LOG) << __func__ << "- Error: Signatures of user ID" << QString::fromUtf8(userId.id()) << "not available";
    }
    return Kleo::any_of(userId.signatures(), [](const auto &certification) {
        return userCanRevokeCertification(certification) == CertificationCanBeRevoked;
    });
}

bool Kleo::userIDBelongsToKey(const GpgME::UserID &userID, const GpgME::Key &key)
{
    return !qstricmp(userID.parent().primaryFingerprint(), key.primaryFingerprint());
}

static time_t creationDate(const GpgME::UserID &uid)
{
    // returns the date of the first self-signature
    for (unsigned int i = 0, numSignatures = uid.numSignatures(); i < numSignatures; ++i) {
        const auto sig = uid.signature(i);
        if (Kleo::isSelfSignature(sig)) {
            return sig.creationTime();
        }
    }
    return 0;
}

bool Kleo::userIDsAreEqual(const GpgME::UserID &lhs, const GpgME::UserID &rhs)
{
    return (qstrcmp(lhs.parent().primaryFingerprint(), rhs.parent().primaryFingerprint()) == 0 //
            && qstrcmp(lhs.id(), rhs.id()) == 0 //
            && creationDate(lhs) == creationDate(rhs));
}
