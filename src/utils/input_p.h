/* -*- mode: c++; c-basic-offset:4 -*-
    utils/input_p.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

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

#ifndef __KLEOPATRA_UTILS_INPUT_P_H__
#define __KLEOPATRA_UTILS_INPUT_P_H__

#include "input.h"
#include "cached.h"

#include <QString>
#include <QIODevice>
#include <KLocalizedString>

namespace Kleo {

class InputImplBase : public Input
{
public:
    InputImplBase() : Input(), m_customLabel(), m_defaultLabel() {}

    QString label() const override
    {
        return m_customLabel.isEmpty() ? m_defaultLabel : m_customLabel;
    }

    void setDefaultLabel(const QString &l)
    {
        m_defaultLabel = l;
    }

    void setLabel(const QString &l) override
    {
        m_customLabel = l;
    }

    QString errorString() const override
    {
        if (m_errorString.dirty()) {
            m_errorString = doErrorString();
        }
        return m_errorString;
    }

private:
    virtual QString doErrorString() const
    {
        if (const std::shared_ptr<QIODevice> io = ioDevice()) {
            return io->errorString();
        } else {
            return i18n("No input device");
        }
    }

private:
    QString m_customLabel;
    QString m_defaultLabel;
    mutable cached<QString> m_errorString;
};

}

#endif
