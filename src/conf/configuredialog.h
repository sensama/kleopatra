/*
    configuredialog.h

    This file is part of Kleopatra
    SPDX-FileCopyrightText: 2000 Espen Sand <espen@kde.org>
    SPDX-FileCopyrightText: 2001-2002 Marc Mutz <mutz@kde.org>
    SPDX-FileCopyrightText: 2004 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-only
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
