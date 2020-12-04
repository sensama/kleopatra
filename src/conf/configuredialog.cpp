/*
    configuredialog.cpp

    This file is part of kleopatra
    SPDX-FileCopyrightText: 2000 Espen Sand <espen@kde.org>
    SPDX-FileCopyrightText: 2001-2002 Marc Mutz <mutz@kde.org>
    SPDX-FileCopyrightText: 2004, 2008 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-only
*/


#include "configuredialog.h"

#include <KConfig>
#include <KLocalizedString>
#include <KConfigGroup>
#include <KSharedConfig>

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
