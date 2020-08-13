/* -*- mode: c++; c-basic-offset:4 -*-
    selftest/gpgagentcheck.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_SELFTEST_GPGAGENTCHECK_H__
#define __KLEOPATRA_SELFTEST_GPGAGENTCHECK_H__

#include <selftest/selftest.h>

#include <memory>

namespace Kleo
{

class SelfTest;

std::shared_ptr<SelfTest> makeGpgAgentConnectivitySelfTest();

}

#endif /* __KLEOPATRA_SELFTEST_GPGAGENTCHECK_H__ */
