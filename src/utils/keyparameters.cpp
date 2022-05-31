/* -*- mode: c++; c-basic-offset:4 -*-
    utils/keyparameters.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "keyparameters.h"

#include <QDate>
#include <QMap>
#include <QUrl>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace GpgME;

namespace
{
    QString encodeDomainName(const QString &domain)
    {
        const QByteArray encodedDomain = QUrl::toAce(domain);
        return encodedDomain.isEmpty() ? domain : QString::fromLatin1(encodedDomain);
    }

    QString encodeEmail(const QString &email)
    {
        const int at = email.lastIndexOf(QLatin1Char('@'));
        if (at < 0) {
            return email;
        }
        return email.left(at + 1) + encodeDomainName(email.mid(at + 1));
    }
}

class KeyParameters::Private
{
    friend class ::Kleo::KeyParameters;

    Protocol protocol;

    Subkey::PubkeyAlgo keyType = Subkey::AlgoUnknown;
    QString cardKeyRef;

    Subkey::PubkeyAlgo subkeyType = Subkey::AlgoUnknown;

    QMap<QString, QStringList> parameters;

public:
    explicit Private(Protocol proto)
        : protocol(proto)
    {
    }

    void setValue(const QString &key, const QString &value)
    {
        parameters[key] = QStringList() << value;
    }

    void addValue(const QString &key, const QString &value)
    {
        parameters[key].push_back(value);
    }
};

KeyParameters::KeyParameters(Protocol protocol)
    : d{new Private{protocol}}
{
}

KeyParameters::~KeyParameters() = default;

KeyParameters::KeyParameters(const KeyParameters &other)
    : d{new Private{*other.d}}
{
}

KeyParameters &KeyParameters::operator=(const KeyParameters &other)
{
    *d = *other.d;
    return *this;
}

KeyParameters::KeyParameters(KeyParameters &&other) = default;

KeyParameters &KeyParameters::operator=(KeyParameters &&other) = default;

void KeyParameters::setKeyType(Subkey::PubkeyAlgo type)
{
    d->keyType = type;
}

GpgME::Subkey::PubkeyAlgo KeyParameters::keyType() const
{
    return d->keyType;
}

void KeyParameters::setCardKeyRef(const QString &cardKeyRef)
{
    d->cardKeyRef = cardKeyRef;
}

QString KeyParameters::cardKeyRef() const
{
    return d->cardKeyRef;
}

void KeyParameters::setKeyLength(unsigned int length)
{
    d->setValue(QStringLiteral("Key-Length"), QString::number(length));
}

void KeyParameters::setKeyCurve(const QString &curve)
{
    d->setValue(QStringLiteral("Key-Curve"), curve);
}

void KeyParameters::setKeyUsages(const QStringList &usages)
{
    d->setValue(QStringLiteral("Key-Usage"), usages.join(QLatin1Char(' ')));
}

void KeyParameters::setSubkeyType(Subkey::PubkeyAlgo type)
{
    d->subkeyType = type;
}

Subkey::PubkeyAlgo KeyParameters::subkeyType() const
{
    return d->subkeyType;
}

void KeyParameters::setSubkeyLength(unsigned int length)
{
    d->setValue(QStringLiteral("Subkey-Length"), QString::number(length));
}

void KeyParameters::setSubkeyCurve(const QString &curve)
{
    d->setValue(QStringLiteral("Subkey-Curve"), curve);
}

void KeyParameters::setSubkeyUsages(const QStringList &usages)
{
    d->setValue(QStringLiteral("Subkey-Usage"), usages.join(QLatin1Char(' ')));
}

void KeyParameters::setExpirationDate(const QDate &date)
{
    d->setValue(QStringLiteral("Expire-Date"), date.toString(Qt::ISODate));
}

void KeyParameters::setName(const QString &name)
{
    d->setValue(QStringLiteral("Name-Real"), name);
}

void KeyParameters::setDN(const QString &dn)
{
    d->setValue(QStringLiteral("Name-DN"), dn);
}

void KeyParameters::setEmail(const QString &email)
{
    d->setValue(QStringLiteral("Name-Email"),
                (d->protocol == CMS) ? encodeEmail(email) : email);
}

void KeyParameters::addEmail(const QString& email)
{
    d->addValue(QStringLiteral("Name-Email"),
                (d->protocol == CMS) ? encodeEmail(email) : email);
}

void KeyParameters::addDomainName(const QString& domain)
{
    d->addValue(QStringLiteral("Name-DNS"), encodeDomainName(domain));
}

void KeyParameters::addURI(const QString& uri)
{
    d->addValue(QStringLiteral("Name-URI"), uri);
}

QString KeyParameters::toString() const
{
    QStringList keyParameters;

    keyParameters.push_back(QLatin1String("<GnupgKeyParms format=\"internal\">"));

    if (d->protocol == OpenPGP) {
        // for backward compatibility with GnuPG 2.0 and earlier
        keyParameters.push_back(QStringLiteral("%ask-passphrase"));
    }

    // add Key-Type as first parameter
    if (!d->cardKeyRef.isEmpty()) {
        keyParameters.push_back(QLatin1String{"Key-Type:card:"} + d->cardKeyRef);
    } else if (d->keyType != Subkey::AlgoUnknown) {
        keyParameters.push_back(QLatin1String{"Key-Type:"} + QString::fromLatin1(Subkey::publicKeyAlgorithmAsString(d->keyType)));
    } else {
        qCWarning(KLEOPATRA_LOG) << "KeyParameters::toString(): Key type is unset/empty";
    }

    if (d->subkeyType != Subkey::AlgoUnknown) {
        keyParameters.push_back(QLatin1String{"Subkey-Type:"} + QString::fromLatin1(Subkey::publicKeyAlgorithmAsString(d->subkeyType)));
    }

    for (auto it = d->parameters.constBegin(); it != d->parameters.constEnd(); ++it) {
        for (const auto &v : it.value()) {
            keyParameters.push_back(it.key() + QLatin1Char(':') + v);
        }
    }

    keyParameters.push_back(QLatin1String("</GnupgKeyParms>"));

    return keyParameters.join(QLatin1Char('\n'));
}
