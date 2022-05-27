/* -*- mode: c++; c-basic-offset:4 -*-
    commands/newcertificatecommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "newcertificatecommand.h"

#include "command_p.h"

#include "newcertificatewizard/newcertificatewizard.h"

#include <settings.h>


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

private:
    void slotDialogAccepted();

private:
    void ensureDialogCreated();

private:
    Protocol protocol = GpgME::UnknownProtocol;
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

void NewCertificateCommand::setProtocol(Protocol proto)
{
    d->protocol = proto;
    if (d->dialog) {
        d->dialog->setProtocol(proto);
    }
}

Protocol NewCertificateCommand::protocol() const
{
    if (d->dialog) {
        return d->dialog->protocol();
    } else {
        return d->protocol;
    }
}

void NewCertificateCommand::doStart()
{
    d->ensureDialogCreated();
    Q_ASSERT(d->dialog);

    const Kleo::Settings settings{};
    const auto cmsAllowed = settings.cmsEnabled() && settings.cmsCertificateCreationAllowed();
    if (d->protocol == UnknownProtocol && !cmsAllowed) {
        d->protocol = GpgME::OpenPGP;
    }
    if (d->protocol != UnknownProtocol) {
        d->dialog->setProtocol(d->protocol);
    }

    d->dialog->show();
}

void NewCertificateCommand::doCancel()
{
    if (d->dialog) {
        d->dialog->close();
    }
}

void NewCertificateCommand::Private::ensureDialogCreated()
{
    if (dialog) {
        return;
    }

    dialog = new NewCertificateWizard;
    applyWindowID(dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &QDialog::accepted, q, [this]() { slotDialogAccepted(); });
    connect(dialog, &QDialog::rejected, q, [this]() { canceled(); });
}

#undef d
#undef q

#include "moc_newcertificatecommand.cpp"
