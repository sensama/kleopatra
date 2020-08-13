/* -*- mode: c++; c-basic-offset:4 -*-
    utils/action_data.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UTILS_ACTIONDATA_H__
#define __KLEOPATRA_UTILS_ACTIONDATA_H__

#include <QString>

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

void make_actions_from_data(const action_data *data, unsigned int numData, KActionCollection *collection);

template <unsigned int N>
inline void make_actions_from_data(const action_data(&data)[N], KActionCollection *collection)
{
    make_actions_from_data(data, N, collection);
}

QAction *make_action_from_data(const action_data &ad, KActionCollection *coll);
QAction *createAction(const action_data &ad, KActionCollection *coll);
}

#endif /* __KLEOPATRA_UTILS_ACTIONDATA_H__ */
