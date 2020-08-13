/* -*- mode: c++; c-basic-offset:4 -*-
    utils/validation.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "validation.h"

#include <utils/multivalidator.h>

#include <KEmailAddress>

#include "kleopatra_debug.h"

#include <QRegExp>


using namespace Kleo;

// these are modeled after gnupg/g10/keygen.c:ask_user_id:
static const QString name_rx = QStringLiteral("[^0-9<>][^<>@]{3,}");
static const QString comment_rx = QStringLiteral("[^()]*");

namespace
{

class EMailValidator : public QValidator
{
public:
    explicit EMailValidator(QObject *parent = nullptr) : QValidator(parent) {}

    State validate(QString &str, int &pos) const override
    {
        Q_UNUSED(pos);
        if (KEmailAddress::isValidSimpleAddress(str)) {
            return Acceptable;
        }
        return Intermediate;
    }
};

}

QValidator *Validation::email(QObject *parent)
{
    return new EMailValidator(parent);
}

QValidator *Validation::email(const QRegExp &addRX, QObject *parent)
{
    return new MultiValidator(email(), new QRegExpValidator(addRX, nullptr), parent);
}

QValidator *Validation::pgpName(QObject *parent)
{
    return new QRegExpValidator(QRegExp(name_rx), parent);
}

QValidator *Validation::pgpName(const QRegExp &addRX, QObject *parent)
{
    return new MultiValidator(pgpName(), new QRegExpValidator(addRX, nullptr), parent);
}

QValidator *Validation::pgpComment(QObject *parent)
{
    return new QRegExpValidator(QRegExp(comment_rx), parent);
}

QValidator *Validation::pgpComment(const QRegExp &addRX, QObject *parent)
{
    return new MultiValidator(pgpComment(), new QRegExpValidator(addRX, nullptr), parent);
}

