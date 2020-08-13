/* -*- mode: c++; c-basic-offset:4 -*-
    utils/gui-helper.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UTILS_GUI_HELPER_H__
#define __KLEOPATRA_UTILS_GUI_HELPER_H__

#include <QAbstractButton>

class QWidget;

namespace Kleo
{
static inline void really_check(QAbstractButton &b, bool on)
{
    const bool excl = b.autoExclusive();
    b.setAutoExclusive(false);
    b.setChecked(on);
    b.setAutoExclusive(excl);
}

static inline bool xconnect(const QObject *a, const char *signal,
                            const QObject *b, const char *slot)
{
    return
        QObject::connect(a, signal, b, slot) &&
        QObject::connect(b, signal, a, slot);
}

/** Aggressively raise a window to foreground. May be platform
 * specific. */
void aggressive_raise(QWidget *w, bool stayOnTop);

}

#endif /* __KLEOPATRA_UTILS_GUI_HELPER_H__ */
