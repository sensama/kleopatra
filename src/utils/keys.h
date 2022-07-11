/* -*- mode: c++; c-basic-offset:4 -*-
    utils/keys.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <gpgme++/key.h>

namespace Kleo
{

struct CertificatePair {
    GpgME::Key openpgp;
    GpgME::Key cms;
};

/** Returns true if \p signature is a self-signature. */
bool isSelfSignature(const GpgME::UserID::Signature &signature);

/**
 * Returns true if the most recent self-signature of \p userId is a revocation
 * signature or if it has expired.
 */
bool isRevokedOrExpired(const GpgME::UserID &userId);

/**
 * Returns true if \p key can be used to certify user IDs, i.e. if the key
 * has the required capability and if the secret key of the (primary)
 * certification subkey is available in the keyring or on a smart card.
 */
bool canCreateCertifications(const GpgME::Key &key);

/**
 * Returns true if \p key can be used for operations requiring the secret key,
 * i.e. if the secret key of the primary key pair is available in the keyring
 * or on a smart card.
 *
 * \note Key::hasSecret() also returns true if a secret key stub, e.g. of an
 * offline key, is available in the keyring.
 */
bool canBeUsedForSecretKeyOperations(const GpgME::Key &key);

/**
 * Returns true if \p userId can be revoked, i.e. if it isn't the last valid
 * user ID of an OpenPGP key.
 */
bool canRevokeUserID(const GpgME::UserID &userId);

/**
 * Returns true if the secret key of the primary key pair of \p key is stored
 * in the keyring.
 */
bool isSecretKeyStoredInKeyRing(const GpgME::Key &key);

}
