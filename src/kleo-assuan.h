/* -*- mode: c++; c-basic-offset:4 -*-
    kleo-assuan.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifndef _ASSUAN_ONLY_GPG_ERRORS
#define _ASSUAN_ONLY_GPG_ERRORS
#endif

#ifdef HAVE_USABLE_ASSUAN
# include <assuan.h>
#else
/*
 * copied from assuan.h:
 */

/* assuan.h - Definitions for the Assuan IPC library
 * SPDX-FileCopyrightText: 2001, 2002, 2003, 2005, 2007 Free Software Foundation Inc.
 *
 * This file is part of Assuan.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifdef _WIN32
typedef void *assuan_fd_t;
#define ASSUAN_INVALID_FD ((void*)(-1))
#define ASSUAN_INT2FD(s)  ((void *)(s))
#define ASSUAN_FD2INT(h)  ((unsigned int)(h))
#else
using assuan_fd_t = int;
#define ASSUAN_INVALID_FD (-1)
#define ASSUAN_INT2FD(s)  ((s))
#define ASSUAN_FD2INT(h)  ((h))
#endif
/*
 * end copied from assuan.h
 */
#endif

