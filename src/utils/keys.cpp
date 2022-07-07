/* -*- mode: c++; c-basic-offset:4 -*-
    utils/keys.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "keys.h"

#include <QByteArray>

// needed for GPGME_VERSION_NUMBER
#include <gpgme.h>

#include <algorithm>
#include <iterator>

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

bool Kleo::isSecretKeyStoredInKeyRing(const GpgME::Key &key)
{
    return key.subkey(0).isSecret() && !key.subkey(0).isCardKey();
}
