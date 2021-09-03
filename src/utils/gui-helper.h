/* -*- mode: c++; c-basic-offset:4 -*-
    utils/gui-helper.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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

/**
 * Puts the second widget after the first widget in the focus order.
 *
 * In contrast to QWidget::setTabOrder(), this function also changes the
 * focus order if the first widget or the second widget has focus policy
 * Qt::NoFocus.
 *
 * Note: After calling this function all widgets in the focus proxy chain
 * of the first widget have focus policy Qt::NoFocus if the first widget
 * has this focus policy. Correspondingly, for the second widget.
 */
void forceSetTabOrder(QWidget *first, QWidget *second);

}
