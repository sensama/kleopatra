/* -*- mode: c++; c-basic-offset:4 -*-
    selftest/registrycheck.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_SELFTEST_REGISTRYCHECK_H__
#define __KLEOPATRA_SELFTEST_REGISTRYCHECK_H__

#include <selftest/selftest.h>

#include <memory>

namespace Kleo
{

class SelfTest;

std::shared_ptr<SelfTest> makeGpgProgramRegistryCheckSelfTest();

}

#endif /* __KLEOPATRA_SELFTEST_REGISTRYCHECK_H__ */
