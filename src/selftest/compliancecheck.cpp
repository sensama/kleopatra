/* -*- mode: c++; c-basic-offset:4 -*-
    selftest/compliancecheck.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "compliancecheck.h"

#include "implementation_p.h"

#include <Libkleo/Formatting>
#include <Libkleo/GnuPG>

#include <KLocalizedString>

using namespace Kleo;
using namespace Kleo::_detail;

namespace
{

class DeVsComplianceCheck : public SelfTestImplementation
{
public:
    explicit DeVsComplianceCheck()
        : SelfTestImplementation(i18nc("@title %1 is a placeholder for the name of a compliance mode. E.g. NATO RESTRICTED compliant or VS-NfD compliant",
                                       "%1?", Formatting::deVsString()))
    {
        runTest();
    }

    void runTest()
    {
        m_skipped = !gnupgUsesDeVsCompliance();
        if (m_skipped) {
            m_explanation =
                xi18nc("@info %1 is a placeholder for the name of a compliance mode. E.g. NATO RESTRICTED compliant or VS-NfD compliant",
                       "<para><application>GnuPG</application> is not configured for %1 communication.</para>", Formatting::deVsString());
            return;
        }

        m_passed = gnupgIsDeVsCompliant();
        if (m_passed) {
            return;
        }

        m_error = Formatting::deVsString(m_passed);
        m_explanation =
            xi18nc("@info %1 is a placeholder for the name of a compliance mode. E.g. NATO RESTRICTED compliant or VS-NfD compliant",
                   "<para>The <application>GnuPG</application> system used by <application>Kleopatra</application> is not %1.</para>", Formatting::deVsString());
        m_proposedFix =
            xi18nc("@info %1 is a placeholder for the name of a compliance mode. E.g. NATO RESTRICTED compliant or VS-NfD compliant",
                   "<para>Install a version of <application>GnuPG</application> that is %1.</para>", Formatting::deVsString());
    }
};

}

std::shared_ptr<SelfTest> Kleo::makeDeVSComplianceCheckSelfTest()
{
    return std::make_shared<DeVsComplianceCheck>();
}
