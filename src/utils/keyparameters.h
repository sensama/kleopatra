/* -*- mode: c++; c-basic-offset:4 -*-
    utils/keyparameters.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <gpgme++/key.h>

#include <memory>

class QDate;
class QString;

namespace Kleo
{
class KeyUsage;

class KeyParameters
{
public:
    enum Protocol {
        OpenPGP,
        CMS
    };

    explicit KeyParameters(Protocol protocol);
    ~KeyParameters();

    KeyParameters(const KeyParameters &other);
    KeyParameters &operator=(const KeyParameters &other);

    KeyParameters(KeyParameters &&other);
    KeyParameters &operator=(KeyParameters &&other);

    void setKeyType(GpgME::Subkey::PubkeyAlgo type);
    GpgME::Subkey::PubkeyAlgo keyType() const;
    void setCardKeyRef(const QString &cardKeyRef);
    QString cardKeyRef() const;
    void setKeyLength(unsigned int length);
    void setKeyCurve(const QString &curve);
    void setKeyUsage(const KeyUsage &usage);
    KeyUsage keyUsage() const;

    void setSubkeyType(GpgME::Subkey::PubkeyAlgo type);
    GpgME::Subkey::PubkeyAlgo subkeyType() const;
    void setSubkeyLength(unsigned int length);
    void setSubkeyCurve(const QString &curve);
    void setSubkeyUsage(const KeyUsage &usage);
    KeyUsage subkeyUsage() const;

    void setExpirationDate(const QDate &date);

    void setName(const QString &name);
    void setDN(const QString &dn);
    void setEmail(const QString &email);
    void addEmail(const QString &email);
    void addDomainName(const QString &domain);
    void addURI(const QString &uri);

    QString toString() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
