/*  crypto/gui/certificatecombobox.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2016 Intevation GmbH

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

#include "certificatecombobox.h"

#include <Libkleo/KeyListModelInterface>

#include <QStylePainter>
#include <QStyleOptionComboBox>
#include <QStyle>

#include <gpgme++/key.h>
using namespace Kleo;

Q_DECLARE_METATYPE(GpgME::Key);

CertificateComboBox::CertificateComboBox(const QIcon &initialIcon,
                                         const QString &initialText,
                                         QWidget *parent)
    : QComboBox(parent),
      m_initialText(initialText),
      m_initialIcon(initialIcon)
{
}

QString CertificateComboBox::initialText() const
{
    return m_initialText;
}

QIcon CertificateComboBox::initialIcon() const
{
    return m_initialIcon;
}

void CertificateComboBox::setInitialText(const QString &txt)
{
    if (txt == m_initialText) {
        return;
    }
    m_initialText = txt;
    if (currentIndex() == -1) {
        update();
    }
}

void CertificateComboBox::setInitialIcon(const QIcon &icon)
{
    if (icon.cacheKey() == m_initialIcon.cacheKey()) {
        return;
    }
    m_initialIcon = icon;
    if (currentIndex() == -1) {
        update();
    }
}

void CertificateComboBox::paintEvent(QPaintEvent *)
{
    QStylePainter p(this);
    p.setPen(palette().color(QPalette::Text));
    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    if (currentIndex() == -1)
    {
        opt.currentText = m_initialText;
        opt.currentIcon = m_initialIcon;
    }
    if (count() > 1) {
        p.drawComplexControl(QStyle::CC_ComboBox, opt);
        p.drawControl(QStyle::CE_ComboBoxLabel, opt);
    } else {
        style()->drawPrimitive (QStyle::PE_Frame, &opt, &p, this);
        style()->drawItemText (&p, rect(), Qt::AlignLeft, palette(), isEnabled(), opt.currentText);
    }
}

void CertificateComboBox::showPopup()
{
    if (count() <= 1) {
        return;
    }
    QComboBox::showPopup();
}

GpgME::Key CertificateComboBox::key() const
{
    return currentData(KeyListModelInterface::KeyRole).value<GpgME::Key>();
}
