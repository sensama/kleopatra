/*
    configuredialog.h

    This file is part of Kleopatra
    Copyright (C) 2000 Espen Sand, espen@kde.org
    Copyright (C) 2001-2002 Marc Mutz <mutz@kde.org>
    Copyright (c) 2004 Klarälvdalens Datakonsult AB
    Copyright (c) 2016 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

    Kleopatra is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License,
    version 2, as published by the Free Software Foundation.

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

#ifndef __KLEOPATRA_CONF_CONFIGUREDIALOG_H__
#define __KLEOPATRA_CONF_CONFIGUREDIALOG_H__

#include <config-kleopatra.h>

/**
 * This is a small wrapper class that holds common code between
 * the KCM Config dialog (which is used when KCMUtils are available)
 * and the KleoPageConfigDialog. Which is just a KPageView
 * with the necessary bits of the KCMultiDialog behavior.
 */
#if HAVE_KCMUTILS
# include <KCMultiDialog>
class ConfigureDialog : public KCMultiDialog
#else
# include "kleopageconfigdialog.h"
class ConfigureDialog : public KleoPageConfigDialog
#endif
{
    Q_OBJECT
public:
    explicit ConfigureDialog(QWidget *parent = nullptr);
    ~ConfigureDialog() override;

protected:
    void hideEvent(QHideEvent *) override;

};

#endif /* __KLEOPATRA_CONF_CONFIGUREDIALOG_H__ */
