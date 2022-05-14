/* -*- mode: c++; c-basic-offset:4 -*-
    commands/changeownertrustcommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "changeownertrustcommand.h"

#include "command_p.h"

#include <dialogs/ownertrustdialog.h>

#include <Libkleo/Formatting>

#include <QGpgME/Protocol>
#include <QGpgME/ChangeOwnerTrustJob>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include "kleopatra_debug.h"


using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;
using namespace GpgME;
using namespace QGpgME;

class ChangeOwnerTrustCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::ChangeOwnerTrustCommand;
    ChangeOwnerTrustCommand *q_func() const
    {
        return static_cast<ChangeOwnerTrustCommand *>(q);
    }
public:
    explicit Private(ChangeOwnerTrustCommand *qq, KeyListController *c);
    ~Private() override;

    void init();

private:
    void slotDialogAccepted();
    void slotDialogRejected();
    void slotResult(const Error &err);

private:
    void ensureDialogCreated();
    void createJob();
    void showErrorDialog(const Error &error);
    void showSuccessDialog();

private:
    QPointer<OwnerTrustDialog> dialog;
    QPointer<ChangeOwnerTrustJob> job;
};

ChangeOwnerTrustCommand::Private *ChangeOwnerTrustCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const ChangeOwnerTrustCommand::Private *ChangeOwnerTrustCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

ChangeOwnerTrustCommand::Private::Private(ChangeOwnerTrustCommand *qq, KeyListController *c)
    : Command::Private(qq, c),
      dialog(),
      job()
{

}

ChangeOwnerTrustCommand::Private::~Private()
{
    qCDebug(KLEOPATRA_LOG);
}

ChangeOwnerTrustCommand::ChangeOwnerTrustCommand(KeyListController *c)
    : Command(new Private(this, c))
{
    d->init();
}

ChangeOwnerTrustCommand::ChangeOwnerTrustCommand(QAbstractItemView *v, KeyListController *c)
    : Command(v, new Private(this, c))
{
    d->init();
}

ChangeOwnerTrustCommand::ChangeOwnerTrustCommand(const Key &key)
    : Command(key, new Private(this, nullptr))
{
    d->init();
}

void ChangeOwnerTrustCommand::Private::init()
{

}

ChangeOwnerTrustCommand::~ChangeOwnerTrustCommand()
{
    qCDebug(KLEOPATRA_LOG);
}

void ChangeOwnerTrustCommand::doStart()
{

    if (d->keys().size() != 1) {
        d->finished();
        return;
    }

    const Key key = d->key();
    if (key.protocol() != GpgME::OpenPGP || (key.hasSecret() && key.ownerTrust() == Key::Ultimate)) {
        d->finished();
        return;
    }

    d->ensureDialogCreated();
    Q_ASSERT(d->dialog);

    d->dialog->setHasSecretKey(key.hasSecret());
    d->dialog->setFormattedCertificateName(Formatting::formatForComboBox(key));
    d->dialog->setOwnerTrust(key.ownerTrust());

    d->dialog->show();

}

void ChangeOwnerTrustCommand::Private::slotDialogAccepted()
{
    Q_ASSERT(dialog);

    const Key::OwnerTrust trust = dialog->ownerTrust();

    qCDebug(KLEOPATRA_LOG) << "trust " << trust;

    createJob();
    Q_ASSERT(job);

    if (const Error err = job->start(key(), trust)) {
        showErrorDialog(err);
        finished();
    }
}

void ChangeOwnerTrustCommand::Private::slotDialogRejected()
{
    Q_EMIT q->canceled();
    finished();
}

void ChangeOwnerTrustCommand::Private::slotResult(const Error &err)
{
    if (err.isCanceled())
        ;
    else if (err) {
        showErrorDialog(err);
    } else {
        showSuccessDialog();
    }
    finished();
}

void ChangeOwnerTrustCommand::doCancel()
{
    qCDebug(KLEOPATRA_LOG);
    if (d->job) {
        d->job->slotCancel();
    }
}

void ChangeOwnerTrustCommand::Private::ensureDialogCreated()
{
    if (dialog) {
        return;
    }

    dialog = new OwnerTrustDialog;
    applyWindowID(dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &QDialog::accepted, q, [this]() { slotDialogAccepted(); });
    connect(dialog, &QDialog::rejected, q, [this]() { slotDialogRejected(); });
}

void ChangeOwnerTrustCommand::Private::createJob()
{
    Q_ASSERT(!job);

    const auto backend = (key().protocol() == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    if (!backend) {
        return;
    }

    ChangeOwnerTrustJob *const j = backend->changeOwnerTrustJob();
    if (!j) {
        return;
    }

    connect(j, &Job::progress,
            q, &Command::progress);
    connect(j, &ChangeOwnerTrustJob::result, q, [this](const GpgME::Error &result) { slotResult(result); });

    job = j;
}

void ChangeOwnerTrustCommand::Private::showErrorDialog(const Error &err)
{
    error(i18n("<p>An error occurred while trying to change "
               "the certification trust for <b>%1</b>:</p><p>%2</p>",
               Formatting::formatForComboBox(key()),
               QString::fromLocal8Bit(err.asString())),
          i18n("Certification Trust Change Error"));
}

void ChangeOwnerTrustCommand::Private::showSuccessDialog()
{
    information(i18n("Certification trust changed successfully."),
                i18n("Certification Trust Change Succeeded"));
}

#undef d
#undef q

#include "moc_changeownertrustcommand.cpp"
