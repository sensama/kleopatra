/* -*- mode: c++; c-basic-offset:4 -*-
    utils/validation.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
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

