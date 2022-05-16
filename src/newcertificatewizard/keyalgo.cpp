/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/keyalgo.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "keyalgo_p.h"

using namespace GpgME;

bool Kleo::NewCertificateUi::is_algo(GpgME::Subkey::PubkeyAlgo algo, KeyAlgo what)
{
    switch (algo) {
        case Subkey::AlgoRSA:
        case Subkey::AlgoRSA_E:
        case Subkey::AlgoRSA_S:
            return what == RSA;
        case Subkey::AlgoELG_E:
        case Subkey::AlgoELG:
            return what == ELG;
        case Subkey::AlgoDSA:
            return what == DSA;
        case Subkey::AlgoECDSA:
            return what == ECDSA;
        case Subkey::AlgoECDH:
            return what == ECDH;
        case Subkey::AlgoEDDSA:
            return what == EDDSA;
        default:
            break;
    }
    return false;
}

bool Kleo::NewCertificateUi::is_rsa(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), RSA);
}

bool Kleo::NewCertificateUi::is_dsa(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), DSA);
}

bool Kleo::NewCertificateUi::is_elg(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), ELG);
}

bool Kleo::NewCertificateUi::is_ecdsa(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), ECDSA);
}

bool Kleo::NewCertificateUi::is_eddsa(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), EDDSA);
}

bool Kleo::NewCertificateUi::is_ecdh(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), ECDH);
}
