/* -*- mode: c++; c-basic-offset:4 -*-
    utils/multivalidator.cpp

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

#include "multivalidator.h"

#include <Libkleo/Stl_Util>

#include <vector>
#include <iterator>

using namespace Kleo;

MultiValidator::~MultiValidator() {}

void MultiValidator::addValidator(QValidator *vali)
{
    if (!vali) {
        return;
    }
    if (!vali->parent()) {
        vali->setParent(this);
    }
    connect(vali, &QObject::destroyed, this, &MultiValidator::_kdmv_slotDestroyed);
    m_validators.push_back(vali);
}

void MultiValidator::addValidators(const QList<QValidator *> &valis)
{
    std::for_each(valis.cbegin(), valis.cend(),
                  [this](QValidator *val) { addValidator(val); });
}

void MultiValidator::removeValidator(QValidator *vali)
{
    if (!vali) {
        return;
    }
    _kdmv_slotDestroyed(vali);
    if (vali->parent() == this) {
        delete vali;
    } else {
        disconnect(vali, &QObject::destroyed, this, &MultiValidator::_kdmv_slotDestroyed);
    }
}

void MultiValidator::removeValidators(const QList<QValidator *> &valis)
{
    std::for_each(valis.cbegin(), valis.cend(),
                  [this](QValidator *val) { removeValidator(val); });
}

void MultiValidator::fixup(QString &str) const
{
    std::for_each(m_validators.begin(), m_validators.end(),
                  [&str](QValidator *val) { val->fixup(str); });
}

QValidator::State MultiValidator::validate(QString &str, int &pos) const
{
    std::vector<State> states;
    states.reserve(m_validators.size());
    std::transform(m_validators.begin(), m_validators.end(),
                   std::back_inserter(states),
                   [&str, &pos](QValidator *val) {
                       return val->validate(str, pos);
                   });
    if (std::any_of(states.cbegin(), states.cend(),
                    [](State state) { return state == Invalid; })) {
        return Invalid;
    }
    if (std::all_of(states.cbegin(), states.cend(),
                    [](State state) { return state == Acceptable; })) {
        return Acceptable;
    }
    return Intermediate;
}

void MultiValidator::_kdmv_slotDestroyed(QObject *o)
{
    m_validators.erase(std::remove(m_validators.begin(), m_validators.end(), o),
                       m_validators.end());
}

