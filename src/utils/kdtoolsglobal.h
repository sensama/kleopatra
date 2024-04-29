/****************************************************************************
** SPDX-FileCopyrightText: 2001-2007 Klarälvdalens Datakonsult AB. All rights reserved.
**
** This file is part of the KD Tools library.
**
** SPDX-License-Identifier: GPL-2.0-only
**
**********************************************************************/

#pragma once

#include <QMutexLocker>

template<typename T>
inline T &__kdtools__dereference_for_methodcall(T &o)
{
    return o;
}

template<typename T>
inline T &__kdtools__dereference_for_methodcall(T *o)
{
    return *o;
}

#define KDAB_SYNCHRONIZED(mutex)                                                                                                                               \
    if (bool __counter_##__LINE__ = false) {                                                                                                                   \
    } else                                                                                                                                                     \
        for (QMutexLocker __locker_##__LINE__(&__kdtools__dereference_for_methodcall(mutex)); !__counter_##__LINE__; __counter_##__LINE__ = true)
