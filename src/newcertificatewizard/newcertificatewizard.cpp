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

#include <settings.h>

#include "chooseprotocolpage_p.h"
#include "enterdetailspage_p.h"
#include "keyalgo_p.h"
#include "keycreationpage_p.h"
#include "resultpage_p.h"

#include "wizardpage_p.h"

#ifdef QGPGME_SUPPORTS_SECRET_KEY_EXPORT
# include "commands/exportsecretkeycommand.h"
#else
# include "commands/exportsecretkeycommand_old.h"
#endif
#include "commands/exportopenpgpcertstoservercommand.h"
#include "commands/exportcertificatecommand.h"

#include "kleopatraapplication.h"

#include "utils/validation.h"
#include "utils/filedialog.h"
#include "utils/keyparameters.h"
#include "utils/userinfo.h"

#include <Libkleo/Compat>
#include <Libkleo/GnuPG>
#include <Libkleo/Stl_Util>
#include <Libkleo/Dn>
#include <Libkleo/OidMap>
#include <Libkleo/KeyCache>
#include <Libkleo/Formatting>

#include <QGpgME/KeyGenerationJob>
#include <QGpgME/Protocol>
#include <QGpgME/CryptoConfig>

#include <gpgme++/global.h>
#include <gpgme++/keygenerationresult.h>
#include <gpgme++/context.h>
#include <gpgme++/interfaces/passphraseprovider.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include "kleopatra_debug.h"
#include <QTemporaryDir>
#include <KMessageBox>
#include <QIcon>

#include <QRegularExpressionValidator>
#include <QLineEdit>
#include <QMetaProperty>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QDesktopServices>
#include <QUrlQuery>

#include <algorithm>

#include <KSharedConfig>
#include <QLocale>

using namespace Kleo;
using namespace Kleo::NewCertificateUi;
using namespace Kleo::Commands;
using namespace GpgME;
#ifndef QGPGME_SUPPORTS_SECRET_KEY_EXPORT
using Kleo::Commands::Compat::ExportSecretKeyCommand;
#endif

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
