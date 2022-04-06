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

#include <QRegularExpression>


using namespace Kleo;

namespace
{

class EMailValidator : public QValidator
{
public:
    explicit EMailValidator(QObject *parent = nullptr) : QValidator(parent) {}

    State validate(QString &str, int &pos) const override
    {
        Q_UNUSED(pos)
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

QValidator *Validation::email(const QString &addRX, QObject *parent)
{
    return new MultiValidator{email(), new QRegularExpressionValidator{QRegularExpression{addRX}, nullptr}, parent};
}

QValidator *Validation::pgpName(QObject *parent)
{
    // this regular expression is modeled after gnupg/g10/keygen.c:ask_user_id:
    static const auto name_rx = QRegularExpression{QStringLiteral("[^0-9<>][^<>@]{3,}")};
    return new QRegularExpressionValidator{name_rx, parent};
}

QValidator *Validation::pgpName(const QString &addRX, QObject *parent)
{
    return new MultiValidator{pgpName(), new QRegularExpressionValidator{QRegularExpression{addRX}, nullptr}, parent};
}

QValidator *Validation::pgpComment(QObject *parent)
{
    // this regular expression is modeled after gnupg/g10/keygen.c:ask_user_id:
    static const auto comment_rx = QRegularExpression{QStringLiteral("[^()]*")};
    return new QRegularExpressionValidator{comment_rx, parent};
}

QValidator *Validation::pgpComment(const QString &addRX, QObject *parent)
{
    return new MultiValidator{pgpComment(), new QRegularExpressionValidator{QRegularExpression{addRX}, nullptr}, parent};
}
