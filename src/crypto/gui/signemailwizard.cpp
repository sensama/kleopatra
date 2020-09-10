/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/signemailwizard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "signemailwizard.h"

#include "signerresolvepage.h"

#include <Libkleo/Formatting>

#include <KLocalizedString>

#include <gpgme++/key.h>


using namespace Kleo;
using namespace Kleo::Crypto::Gui;
using namespace GpgME;

namespace
{

class SignerResolveValidator : public SignerResolvePage::Validator
{
public:
    explicit SignerResolveValidator(SignerResolvePage *page);
    bool isComplete() const override;
    QString explanation() const override;
    void update() const;
    QString customWindowTitle() const override
    {
        return QString();
    }

private:
    SignerResolvePage *m_page;
    mutable QString expl;
    mutable bool complete;
};
}

SignerResolveValidator::SignerResolveValidator(SignerResolvePage *page)
    : SignerResolvePage::Validator(), m_page(page), complete(true)
{
    Q_ASSERT(m_page);
}

void SignerResolveValidator::update() const
{
    const bool haveSelected = !m_page->selectedProtocols().empty();
    const std::vector<Protocol> missing = m_page->selectedProtocolsWithoutSigningCertificate();

    complete = haveSelected && missing.empty();
    expl.clear();
    if (complete) {
        return;
    }
    if (!haveSelected) {
        expl = i18n("You need to select a signing certificate to proceed.");
        return;
    }

    Q_ASSERT(missing.size() <= 2);
    if (missing.size() == 1) {
        expl = i18n("You need to select an %1 signing certificate to proceed.", Formatting::displayName(missing[0]));
    } else {
        expl = i18n("You need to select %1 and %2 signing certificates to proceed.", Formatting::displayName(missing[0]), Formatting::displayName(missing[1]));
    }
}

QString SignerResolveValidator::explanation() const
{
    update();
    return expl;
}

bool SignerResolveValidator::isComplete() const
{
    update();
    return complete;
}

class SignEMailWizard::Private
{
    friend class ::Kleo::Crypto::Gui::SignEMailWizard;
    SignEMailWizard *const q;
public:
    explicit Private(SignEMailWizard *qq);
    ~Private();

    void operationSelected();

    bool m_quickMode;
};

SignEMailWizard::Private::Private(SignEMailWizard *qq)
    : q(qq), m_quickMode(false)
{
    q->setWindowTitle(i18nc("@title:window", "Sign Mail Message"));

    std::vector<int> pageOrder;
    q->setSignerResolvePageValidator(std::shared_ptr<SignerResolveValidator>(new SignerResolveValidator(q->signerResolvePage())));
    pageOrder.push_back(SignEncryptWizard::ResolveSignerPage);
    pageOrder.push_back(SignEncryptWizard::ResultPage);
    q->setPageOrder(pageOrder);
    q->setCommitPage(SignEncryptWizard::ResolveSignerPage);
    q->setEncryptionSelected(false);
    q->setEncryptionUserMutable(false);
    q->setSigningSelected(true);
    q->setSigningUserMutable(false);
    q->signerResolvePage()->setProtocolSelectionUserMutable(false);
    q->setMultipleProtocolsAllowed(false);
}

SignEMailWizard::Private::~Private() {}

SignEMailWizard::SignEMailWizard(QWidget *parent, Qt::WindowFlags f)
    : SignEncryptWizard(parent, f), d(new Private(this))
{
}

bool SignEMailWizard::quickMode() const
{
    return d->m_quickMode;
}

void SignEMailWizard::setQuickMode(bool quick)
{
    if (quick == d->m_quickMode) {
        return;
    }
    d->m_quickMode = quick;
    signerResolvePage()->setAutoAdvance(quick);
    setKeepResultPageOpenWhenDone(!quick);
}

SignEMailWizard::~SignEMailWizard() {}

