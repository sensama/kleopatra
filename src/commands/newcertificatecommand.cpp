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

#include "dialogs/choosecertificateprotocoldialog.h"
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

    void chooseProtocol();
    void createCertificate();

private:
    void onProtocolChosen();

    void slotDialogAccepted();

private:
    void ensureDialogCreated();

private:
    Protocol protocol = GpgME::UnknownProtocol;
    QPointer<ChooseCertificateProtocolDialog> protocolDialog;
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

void NewCertificateCommand::Private::chooseProtocol()
{
    Q_ASSERT(protocol == GpgME::UnknownProtocol);
    Q_ASSERT(!protocolDialog);

    protocolDialog = new ChooseCertificateProtocolDialog;
    applyWindowID(protocolDialog);

    connect(protocolDialog, &QDialog::accepted, q, [this]() {
        onProtocolChosen();
    });
    connect(protocolDialog, &QDialog::rejected, q, [this]() {
        canceled();
        protocolDialog->deleteLater();
    });

    protocolDialog->show();
}

void NewCertificateCommand::Private::onProtocolChosen()
{
    protocol = protocolDialog->protocol();
    protocolDialog->deleteLater();

    createCertificate();
}

void Kleo::Commands::NewCertificateCommand::Private::createCertificate()
{
    Q_ASSERT(protocol != GpgME::UnknownProtocol);

    if (protocol == GpgME::OpenPGP) {
        auto cmd = new NewOpenPGPCertificateCommand{view(), controller()};
        if (parentWidgetOrView() != view()) {
            cmd->setParentWidget(parentWidgetOrView());
        }
        cmd->setParentWId(parentWId());
        connect(cmd, &NewOpenPGPCertificateCommand::finished,
                q, [this]() { finished(); });
        connect(cmd, &NewOpenPGPCertificateCommand::canceled,
                q, [this]() { canceled(); });
        cmd->start();
    } else {
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

        dialog->setProtocol(protocol);
        dialog->show();
    }
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

void NewCertificateCommand::setProtocol(Protocol proto)
{
    d->protocol = proto;
}

Protocol NewCertificateCommand::protocol() const
{
    return d->protocol;
}

void NewCertificateCommand::doStart()
{
    const Kleo::Settings settings{};
    const auto cmsAllowed = settings.cmsEnabled() && settings.cmsCertificateCreationAllowed();
    if (d->protocol == UnknownProtocol && !cmsAllowed) {
        d->protocol = GpgME::OpenPGP;
    }

    if (d->protocol == UnknownProtocol) {
        d->chooseProtocol();
    } else {
        d->createCertificate();
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
