/* -*- mode: c++; c-basic-offset:4 -*-
    utils/action_data.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <vector>

class QObject;
class QAction;
class KActionCollection;

namespace Kleo
{

struct action_data {
    const char *name;
    QString text;
    QString tooltip;
    const char *icon;
    const QObject *receiver;
    const char *slot;
    QString shortcut;
    bool toggle;
    bool enabled;
};

void make_actions_from_data(const std::vector<action_data> &data, KActionCollection *collection);

QAction *make_action_from_data(const action_data &ad, KActionCollection *coll);
QAction *createAction(const action_data &ad, KActionCollection *coll);
}

