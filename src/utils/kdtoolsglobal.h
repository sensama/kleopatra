/****************************************************************************
** SPDX-FileCopyrightText: 2001-2007 Klarälvdalens Datakonsult AB. All rights reserved.
**
** This file is part of the KD Tools library.
**
** SPDX-License-Identifier: GPL-2.0-only
**
**********************************************************************/

#pragma once

#include <qglobal.h>

#define KDAB_DISABLE_COPY( x ) private: x( const x & ); x & operator=( const x & )

#ifdef DOXYGEN_RUN
# define KDAB_IMPLEMENT_SAFE_BOOL_OPERATOR( func ) operator unspecified_bool_type() const { return func; }
#else
# define KDAB_IMPLEMENT_SAFE_BOOL_OPERATOR( func )                      \
    private: struct __safe_bool_dummy__ { void nonnull(); };            \
    typedef void ( __safe_bool_dummy__::*unspecified_bool_type )(); \
    public:                                                             \
    operator unspecified_bool_type() const {                        \
        return ( func ) ? &__safe_bool_dummy__::nonnull : 0 ;       \
    }
#endif

#define KDTOOLS_MAKE_RELATION_OPERATORS( Class, linkage )             \
    linkage bool operator>( const Class & lhs, const Class & rhs ) {  \
        return operator<( lhs, rhs );                                 \
    }                                                                 \
    linkage bool operator!=( const Class & lhs, const Class & rhs ) { \
        return !operator==( lhs, rhs );                               \
    }                                                                 \
    linkage bool operator<=( const Class & lhs, const Class & rhs ) { \
        return !operator>( lhs, rhs );                                \
    }                                                                 \
    linkage bool operator>=( const Class & lhs, const Class & rhs ) { \
        return !operator<( lhs, rhs );                                \
    }

template <typename T>
inline T &__kdtools__dereference_for_methodcall(T &o)
{
    return o;
}

template <typename T>
inline T &__kdtools__dereference_for_methodcall(T *o)
{
    return *o;
}

#define KDAB_SET_OBJECT_NAME( x ) __kdtools__dereference_for_methodcall( x ).setObjectName( QStringLiteral( #x ) )

#define KDAB_SYNCHRONIZED( mutex ) if ( bool __counter_##__LINE__ = false ) {} else \
        for ( QMutexLocker __locker_##__LINE__( &__kdtools__dereference_for_methodcall( mutex ) ) ; !__counter_##__LINE__ ; __counter_##__LINE__ = true )


