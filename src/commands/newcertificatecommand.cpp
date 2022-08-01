/* -*- mode: c++; c-basic-offset:4 -*-
    commands/newcertificatecommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "newcertificatecommand.h"

#include "command_p.h"
#include "newopenpgpcertificatecommand.h"

#include "newcertificatewizard/newcertificatewizard.h"

#include <settings.h>

#include <kleopatra_debug.h>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

class NewCertificateCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::NewCertificateCommand;
    NewCertificateCommand *q_func() const
    {
        return static_cast<NewCertificateCommand *>(q);
    }
public:
    explicit Private(NewCertificateCommand *qq, KeyListController *c)
        : Command::Private{qq, c}
    {
    }

    void createCertificate();

private:
    void slotDialogAccepted();

private:
    void ensureDialogCreated();

private:
    QPointer<NewCertificateWizard> dialog;
};

NewCertificateCommand::Private *NewCertificateCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const NewCertificateCommand::Private *NewCertificateCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

void NewCertificateCommand::Private::createCertificate()
{
    Q_ASSERT(!dialog);

    dialog = new NewCertificateWizard;
    applyWindowID(dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &QDialog::accepted, q, [this]() {
        slotDialogAccepted();
    });
    connect(dialog, &QDialog::rejected, q, [this]() {
        canceled();
    });

    dialog->setProtocol(GpgME::CMS);
    dialog->show();
}

void NewCertificateCommand::Private::slotDialogAccepted()
{
    finished();
}

NewCertificateCommand::NewCertificateCommand()
    : Command(new Private(this, nullptr))
{
}

NewCertificateCommand::NewCertificateCommand(KeyListController *c)
    : Command(new Private(this, c))
{
}

NewCertificateCommand::NewCertificateCommand(QAbstractItemView *v, KeyListController *c)
    : Command(v, new Private(this, c))
{
}

NewCertificateCommand::~NewCertificateCommand() = default;

void NewCertificateCommand::doStart()
{
    const Kleo::Settings settings{};
    if (settings.cmsEnabled() && settings.cmsCertificateCreationAllowed()) {
        d->createCertificate();
    } else {
        d->error(i18n("You are not allowed to create S/MIME certificate signing requests."));
        d->finished();
    }
}

void NewCertificateCommand::doCancel()
{
    if (d->dialog) {
        d->dialog->close();
    }
}

#undef d
#undef q

#include "moc_newcertificatecommand.cpp"
