/* -*- mode: c++; c-basic-offset:4 -*-
    utils/keys.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "keys.h"

#include <QByteArray>

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
    // Key::hasSecret() is also true for offline keys (i.e. keys with a secret key stub that are not stored on a card),
    // but those keys cannot be used for certifications; therefore, we check whether the primary subkey has a proper secret key
    // or whether its secret key is stored on a card, so that gpg can ask for the card.
    return key.canCertify() && (key.subkey(0).isSecret() || key.subkey(0).isCardKey());
}
