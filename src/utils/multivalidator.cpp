/* -*- mode: c++; c-basic-offset:4 -*-
    utils/multivalidator.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
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

