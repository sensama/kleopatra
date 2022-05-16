/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/keyalgo_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <gpgme++/key.h>

namespace Kleo::NewCertificateUi
{

enum KeyAlgo { RSA, DSA, ELG, ECDSA, ECDH, EDDSA };

bool is_algo(GpgME::Subkey::PubkeyAlgo algo, KeyAlgo what);

bool is_rsa(unsigned int algo);
bool is_dsa(unsigned int algo);
bool is_elg(unsigned int algo);
bool is_ecdsa(unsigned int algo);
bool is_eddsa(unsigned int algo);
bool is_ecdh(unsigned int algo);

}
