/* -*- mode: c++; c-basic-offset:4 -*-
    commands/createcsrforcardkeycommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "createcsrforcardkeycommand.h"

#include "cardcommand_p.h"

#include "dialogs/createcsrforcardkeydialog.h"
#include "dialogs/csrcreationresultdialog.h"

#include "smartcard/pivcard.h"
#include "smartcard/readerstatus.h"

#include "utils/keyparameters.h"

#include <Libkleo/Formatting>

#include <KLocalizedString>

#include <QUrl>

#include <QGpgME/Protocol>
#include <QGpgME/KeyGenerationJob>

#include <gpgme++/context.h>
#include <gpgme++/engineinfo.h>
#include <gpgme++/keygenerationresult.h>

#include <gpgme.h>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;
using namespace Kleo::SmartCard;
using namespace GpgME;
using namespace QGpgME;

class CreateCSRForCardKeyCommand::Private : public CardCommand::Private
{
    friend class ::Kleo::Commands::CreateCSRForCardKeyCommand;
    CreateCSRForCardKeyCommand *q_func() const
    {
        return static_cast<CreateCSRForCardKeyCommand *>(q);
    }
public:
    explicit Private(CreateCSRForCardKeyCommand *qq,
                     const std::string &keyRef, const std::string &serialNumber, const std::string &appName, QWidget *parent);
    ~Private();

private:
    void start();

    void slotDialogAccepted();
    void slotDialogRejected();
    void slotResult(const KeyGenerationResult &result, const QByteArray &request);

    void ensureDialogCreated();

private:
    std::string appName;
    std::string keyRef;
    QStringList keyUsages;
    QPointer<CreateCSRForCardKeyDialog> dialog;
};

CreateCSRForCardKeyCommand::Private *CreateCSRForCardKeyCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const CreateCSRForCardKeyCommand::Private *CreateCSRForCardKeyCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

CreateCSRForCardKeyCommand::Private::Private(CreateCSRForCardKeyCommand *qq,
                                             const std::string &keyRef_, const std::string &serialNumber, const std::string &appName_, QWidget *parent)
    : CardCommand::Private(qq, serialNumber, parent)
    , appName(appName_)
    , keyRef(keyRef_)
{
}

CreateCSRForCardKeyCommand::Private::~Private()
{
}

namespace
{
QStringList getKeyUsages(const KeyPairInfo &keyInfo)
{
    // note: gpgsm does not support creating CSRs for authentication certificates
    QStringList usages;
    if (keyInfo.canCertify()) {
        usages.push_back(QStringLiteral("cert"));
    }
    if (keyInfo.canSign()) {
        usages.push_back(QStringLiteral("sign"));
    }
    if (keyInfo.canEncrypt()) {
        usages.push_back(QStringLiteral("encrypt"));
    }
    return usages;
}
}

void CreateCSRForCardKeyCommand::Private::start()
{
    if (appName != PIVCard::AppName) {
        qCWarning(KLEOPATRA_LOG) << "CreateCSRForCardKeyCommand does not support card application" << QString::fromStdString(appName);
        finished();
        return;
    }

    const auto card = ReaderStatus::instance()->getCard(serialNumber(), appName);
    if (!card) {
        error(i18n("Failed to find the smartcard with the serial number: %1", QString::fromStdString(serialNumber())));
        finished();
        return;
    }

    const KeyPairInfo &keyInfo = card->keyInfo(keyRef);
    keyUsages = getKeyUsages(keyInfo);

    ensureDialogCreated();

    dialog->setWindowTitle(i18n("Certificate Details"));
    dialog->setName(card->cardHolder());

    dialog->show();
}

void CreateCSRForCardKeyCommand::Private::slotDialogAccepted()
{
    const Error err = ReaderStatus::switchCardAndApp(serialNumber(), appName);
    if (err) {
        finished();
        return;
    }

    const auto backend = smime();
    if (!backend) {
        finished();
        return;
    }

    KeyGenerationJob *const job = backend->keyGenerationJob();
    if (!job) {
        finished();
        return;
    }

    Job::context(job)->setArmor(true);

    connect(job, SIGNAL(result(const GpgME::KeyGenerationResult &, const QByteArray &)),
            q, SLOT(slotResult(const GpgME::KeyGenerationResult &, const QByteArray &)));

    KeyParameters keyParameters(KeyParameters::CMS);
    keyParameters.setKeyType(QString::fromStdString(keyRef));
    keyParameters.setKeyUsages(keyUsages);
    keyParameters.setDN(dialog->dn());
    keyParameters.setEmail(dialog->email());

    if (const Error err = job->start(keyParameters.toString())) {
        error(i18nc("@info", "Creating a CSR for the card key failed:\n%1", QString::fromUtf8(err.asString())),
              i18nc("@title", "Error"));
        finished();
    }
}

void CreateCSRForCardKeyCommand::Private::slotDialogRejected()
{
    canceled();
}

void CreateCSRForCardKeyCommand::Private::slotResult(const KeyGenerationResult &result, const QByteArray &request)
{
    if (result.error().isCanceled()) {
        // do nothing
    } else if (result.error()) {
        error(i18nc("@info", "Creating a CSR for the card key failed:\n%1", QString::fromUtf8(result.error().asString())),
              i18nc("@title", "Error"));
    } else {
        auto resultDialog = new CSRCreationResultDialog;
        applyWindowID(resultDialog);
        resultDialog->setAttribute(Qt::WA_DeleteOnClose);
        resultDialog->setCSR(request);
        resultDialog->show();
    }

    finished();
}

void CreateCSRForCardKeyCommand::Private::ensureDialogCreated()
{
    if (dialog) {
        return;
    }

    dialog = new CreateCSRForCardKeyDialog;
    applyWindowID(dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, SIGNAL(accepted()), q, SLOT(slotDialogAccepted()));
    connect(dialog, SIGNAL(rejected()), q, SLOT(slotDialogRejected()));
}

CreateCSRForCardKeyCommand::CreateCSRForCardKeyCommand(const std::string &keyRef, const std::string &serialNumber, const std::string &appName, QWidget *parent)
    : CardCommand(new Private(this, keyRef, serialNumber, appName, parent))
{
}

CreateCSRForCardKeyCommand::~CreateCSRForCardKeyCommand()
{
}

void CreateCSRForCardKeyCommand::doStart()
{
    d->start();
}

void CreateCSRForCardKeyCommand::doCancel()
{
}

#undef d
#undef q

#include "moc_createcsrforcardkeycommand.cpp"
