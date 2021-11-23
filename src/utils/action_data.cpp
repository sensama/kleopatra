/* -*- mode: c++; c-basic-offset:4 -*-
    utils/action_data.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "action_data.h"

#include <KToggleAction>
#include <KActionCollection>
#include <QAction>
#include <QIcon>
#include <QKeySequence>

QAction *Kleo::createAction(const action_data &ad, KActionCollection *coll)
{

    QAction *const a = ad.toggle ? new KToggleAction(coll) : new QAction(coll);
    a->setObjectName(QLatin1String(ad.name));
    a->setText(ad.text);
    if (!ad.tooltip.isEmpty()) {
        a->setToolTip(ad.tooltip);
    }
    if (ad.icon) {
        a->setIcon(QIcon::fromTheme(QLatin1String(ad.icon)));
    }
    if (ad.receiver && ad.slot) {
        if (ad.toggle) {
            QObject::connect(a, SIGNAL(toggled(bool)), ad.receiver, ad.slot);
        } else {
            QObject::connect(a, SIGNAL(triggered()), ad.receiver, ad.slot);
        }
    }
    a->setEnabled(ad.enabled);
    coll->addAction(QLatin1String(ad.name), a);
    return a;
}

QAction *Kleo::make_action_from_data(const action_data &ad, KActionCollection *coll)
{

    QAction *const a = createAction(ad, coll);
    if (!ad.shortcut.isEmpty()) {
        coll->setDefaultShortcut(a, QKeySequence(ad.shortcut));
    }
    return a;
}

void Kleo::make_actions_from_data(const std::vector<action_data> &data, KActionCollection *coll)
{
    for (const auto &actionData : data) {
        coll->addAction(QLatin1String(actionData.name), make_action_from_data(actionData, coll));
    }
}
