/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/encryptemailwizard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "encryptemailwizard.h"
#include <crypto/gui/resolverecipientspage.h>

#include <KLocalizedString>

using namespace Kleo;
using namespace Kleo::Crypto;
using namespace Kleo::Crypto::Gui;

class EncryptEMailWizard::Private
{
public:
    Private() : m_quickMode(false) {}
    bool m_quickMode;
};

EncryptEMailWizard::EncryptEMailWizard(QWidget *parent, Qt::WindowFlags flags) : SignEncryptWizard(parent, flags), d(new Private)
{
    setWindowTitle(i18nc("@title:window", "Encrypt Mail Message"));
    std::vector<int> pageOrder;
    pageOrder.push_back(ResolveRecipientsPage);
    pageOrder.push_back(ResultPage);
    setPageOrder(pageOrder);
    setCommitPage(SignEncryptWizard::ResolveRecipientsPage);
}

EncryptEMailWizard::~EncryptEMailWizard()
{

}

bool EncryptEMailWizard::quickMode() const
{
    return d->m_quickMode;
}

void EncryptEMailWizard::setQuickMode(bool quick)
{
    if (quick == d->m_quickMode) {
        return;
    }
    d->m_quickMode = quick;
    signerResolvePage()->setAutoAdvance(quick);
    resolveRecipientsPage()->setAutoAdvance(quick);
    setKeepResultPageOpenWhenDone(!quick);
}

#include "encryptemailwizard.h"
