/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/newcertificatewizard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "newcertificatewizard.h"

#include "chooseprotocolpage_p.h"
#include "enterdetailspage_p.h"
#include "keycreationpage_p.h"
#include "resultpage_p.h"

#include <KLocalizedString>

#include <QDir>
#include <QTemporaryDir>

using namespace Kleo;
using namespace Kleo::NewCertificateUi;
using namespace GpgME;

class NewCertificateWizard::Private
{
    friend class ::Kleo::NewCertificateWizard;
    NewCertificateWizard *const q;
public:
    explicit Private(NewCertificateWizard *qq)
        : q(qq),
          tmp(QDir::temp().absoluteFilePath(QStringLiteral("kleo-"))),
          ui(q)
    {
        q->setWindowTitle(i18nc("@title:window", "Key Pair Creation Wizard"));
    }

private:
    GpgME::Protocol initialProtocol = GpgME::UnknownProtocol;
    QTemporaryDir tmp;
    struct Ui {
        ChooseProtocolPage chooseProtocolPage;
        EnterDetailsPage enterDetailsPage;
        KeyCreationPage keyCreationPage;
        ResultPage resultPage;

        explicit Ui(NewCertificateWizard *q)
            : chooseProtocolPage(q),
              enterDetailsPage(q),
              keyCreationPage(q),
              resultPage(q)
        {
            KDAB_SET_OBJECT_NAME(chooseProtocolPage);
            KDAB_SET_OBJECT_NAME(enterDetailsPage);
            KDAB_SET_OBJECT_NAME(keyCreationPage);
            KDAB_SET_OBJECT_NAME(resultPage);

            q->setOptions(NoBackButtonOnStartPage|DisabledBackButtonOnLastPage);

            q->setPage(ChooseProtocolPageId, &chooseProtocolPage);
            q->setPage(EnterDetailsPageId,   &enterDetailsPage);
            q->setPage(KeyCreationPageId,    &keyCreationPage);
            q->setPage(ResultPageId,         &resultPage);

            q->setStartId(ChooseProtocolPageId);
        }

    } ui;

};

NewCertificateWizard::NewCertificateWizard(QWidget *p)
    : QWizard(p), d(new Private(this))
{
}

NewCertificateWizard::~NewCertificateWizard() {}

void NewCertificateWizard::showEvent(QShowEvent *event)
{
    // set WA_KeyboardFocusChange attribute to force visual focus of the
    // focussed button when the wizard is shown (required for Breeze style
    // and some other styles)
    window()->setAttribute(Qt::WA_KeyboardFocusChange);
    QWizard::showEvent(event);
}

void NewCertificateWizard::setProtocol(Protocol proto)
{
    d->initialProtocol = proto;
    d->ui.chooseProtocolPage.setProtocol(proto);
    setStartId(proto == UnknownProtocol ? ChooseProtocolPageId : EnterDetailsPageId);
}

Protocol NewCertificateWizard::protocol() const
{
    return d->ui.chooseProtocolPage.protocol();
}

void NewCertificateWizard::resetProtocol()
{
    d->ui.chooseProtocolPage.setProtocol(d->initialProtocol);
}

void NewCertificateWizard::restartAtEnterDetailsPage()
{
    const auto protocol = d->ui.chooseProtocolPage.protocol();
    restart();  // resets the protocol to the initial protocol
    d->ui.chooseProtocolPage.setProtocol(protocol);
    while (currentId() != NewCertificateWizard::EnterDetailsPageId) {
        next();
    }
}

QDir NewCertificateWizard::tmpDir() const
{
    return QDir(d->tmp.path());
}
