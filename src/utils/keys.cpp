/* -*- mode: c++; c-basic-offset:4 -*-
    utils/keys.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "keys.h"

#include <Libkleo/Algorithm>
#include <Libkleo/KeyCache>

#include <QByteArray>

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
    const int numberOfValidUserIds = std::count_if(std::begin(userIds), std::end(userIds),
                                                   [](const auto &u) {
                                                       return !Kleo::isRevokedOrExpired(u);
                                                   });
    return numberOfValidUserIds == 1;
}
}

bool Kleo::isSelfSignature(const GpgME::UserID::Signature &signature)
{
    return !qstrcmp(signature.parent().parent().keyID(), signature.signerKeyID());
}

bool Kleo::isRevokedOrExpired(const GpgME::UserID &userId)
{
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
            && userId.parent().protocol() == GpgME::OpenPGP
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
    return Kleo::any_of(userId.signatures(), [](const auto &certification) {
        return userCanRevokeCertification(certification) == CertificationCanBeRevoked;
    });
}
