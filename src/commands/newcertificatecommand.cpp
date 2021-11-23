/* -*- mode: c++; c-basic-offset:4 -*-
    commands/newcertificatecommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "newcertificatecommand.h"

#include "command_p.h"

#include <settings.h>

#include <newcertificatewizard/newcertificatewizard.h>


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
    explicit Private(NewCertificateCommand *qq, KeyListController *c);
    ~Private() override;

    void init();

private:
    void slotDialogRejected();
    void slotDialogAccepted();

private:
    void ensureDialogCreated();

private:
    Protocol protocol;
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

NewCertificateCommand::Private::Private(NewCertificateCommand *qq, KeyListController *c)
    : Command::Private(qq, c),
      protocol(UnknownProtocol),
      dialog()
{

}

NewCertificateCommand::Private::~Private() {}

NewCertificateCommand::NewCertificateCommand()
    : Command(new Private(this, nullptr))
{
    d->init();
}

NewCertificateCommand::NewCertificateCommand(KeyListController *c)
    : Command(new Private(this, c))
{
    d->init();
}

NewCertificateCommand::NewCertificateCommand(QAbstractItemView *v, KeyListController *c)
    : Command(v, new Private(this, c))
{
    d->init();
}

void NewCertificateCommand::Private::init()
{

}

NewCertificateCommand::~NewCertificateCommand() {}

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

    if (d->protocol == UnknownProtocol && !Settings{}.cmsEnabled()) {
        d->protocol = GpgME::OpenPGP;
    }
    if (d->protocol != UnknownProtocol) {
        d->dialog->setProtocol(d->protocol);
    }

    d->dialog->show();
}

void NewCertificateCommand::Private::slotDialogRejected()
{
    Q_EMIT q->canceled();
    finished();
}

void NewCertificateCommand::Private::slotDialogAccepted()
{
    finished();
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

    connect(dialog, SIGNAL(rejected()), q, SLOT(slotDialogRejected()));
    connect(dialog, SIGNAL(accepted()), q, SLOT(slotDialogAccepted()));
}

#undef d
#undef q

#include "moc_newcertificatecommand.cpp"
