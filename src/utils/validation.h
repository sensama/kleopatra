/* -*- mode: c++; c-basic-offset:4 -*-
    utils/validation.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

class QObject;
class QString;
class QValidator;

namespace Kleo
{
namespace Validation
{

enum Flags {
    Optional,
    Required
};

QValidator *email(Flags flags = Required, QObject *parent = nullptr);
QValidator *pgpName(Flags flags = Required, QObject *parent = nullptr);
QValidator *pgpComment(Flags flags = Required, QObject *parent = nullptr);

QValidator *email(const QString &additionalRegExp, Flags flags = Required, QObject *parent = nullptr);
QValidator *pgpName(const QString &additionalRegExp, Flags flags = Required, QObject *parent = nullptr);
QValidator *pgpComment(const QString &additionalRegExp, Flags flags = Required, QObject *parent = nullptr);

}
}
