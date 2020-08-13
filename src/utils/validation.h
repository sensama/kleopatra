/* -*- mode: c++; c-basic-offset:4 -*-
    utils/validation.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UTILS_VALIDATION_H__
#define __KLEOPATRA_UTILS_VALIDATION_H__

#include <QObject>
class QValidator;
class QRegExp;

namespace Kleo
{
namespace Validation
{

QValidator *email(QObject *parent = nullptr);
QValidator *pgpName(QObject *parent = nullptr);
QValidator *pgpComment(QObject *parent = nullptr);

QValidator *email(const QRegExp &additionalRegExp, QObject *parent = nullptr);
QValidator *pgpName(const QRegExp &additionalRegExp, QObject *parent = nullptr);
QValidator *pgpComment(const QRegExp &additionalRegExp, QObject *parent = nullptr);

}
}

#endif /* __KLEOPATRA_UTILS_VALIDATION_H__ */
