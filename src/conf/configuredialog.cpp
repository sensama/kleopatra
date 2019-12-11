/*
    configuredialog.cpp

    This file is part of kleopatra
    Copyright (C) 2000 Espen Sand, espen@kde.org
    Copyright (C) 2001-2002 Marc Mutz <mutz@kde.org>
    Copyright (c) 2004,2008 Klarälvdalens Datakonsult AB
    2016 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

    Libkleopatra is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License,
    version 2, as published by the Free Software Foundation.

    Libkleopatra is distributed in the hope that it will be useful,
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

#include "configuredialog.h"

#include <KConfig>
#include <KLocalizedString>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QIcon>
#include <QApplication>

#if HAVE_KCMUTILS
# include <KCMultiDialog>
#else
# include "kleopageconfigdialog.h"
#endif

ConfigureDialog::ConfigureDialog(QWidget *parent)
#if HAVE_KCMUTILS
    : KCMultiDialog(parent)
#else
    : KleoPageConfigDialog(parent)
#endif
{
    setFaceType(KPageDialog::List);
    setWindowTitle(i18nc("@title:window", "Configure"));
    addModule(QStringLiteral("kleopatra_config_dirserv"));
    addModule(QStringLiteral("kleopatra_config_appear"));
    addModule(QStringLiteral("kleopatra_config_cryptooperations"));
    addModule(QStringLiteral("kleopatra_config_smimevalidation"));
    addModule(QStringLiteral("kleopatra_config_gnupgsystem"));
    // We store the minimum size of the dialog on hide, because otherwise
    // the KCMultiDialog starts with the size of the first kcm, not
    // the largest one. This way at least after the first showing of
    // the largest kcm the size is kept.
    const KConfigGroup geometry(KSharedConfig::openConfig(), "Geometry");
    const int width = geometry.readEntry("ConfigureDialogWidth", 0);
    const int height = geometry.readEntry("ConfigureDialogHeight", 0);
    if (width != 0 && height != 0) {
        setMinimumSize(width, height);
    }
}

void ConfigureDialog::hideEvent(QHideEvent *e)
{
    const QSize minSize = minimumSizeHint();
    KConfigGroup geometry(KSharedConfig::openConfig(), "Geometry");
    geometry.writeEntry("ConfigureDialogWidth", minSize.width());
    geometry.writeEntry("ConfigureDialogHeight", minSize.height());
#if HAVE_KCMUTILS
    KCMultiDialog::hideEvent(e);
#else
    KleoPageConfigDialog::hideEvent(e);
#endif
}

ConfigureDialog::~ConfigureDialog()
{
}
