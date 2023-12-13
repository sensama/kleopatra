/*
    SPDX-FileCopyrightText: 2023 g10 Code GmbH
    SPDX-FileContributor: Tobias Fella <tobias.fella@gnupg.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "addsubkeycommand.h"

#include "command_p.h"
#include "dialogs/addsubkeydialog.h"

#include <Libkleo/Formatting>

#include <KLocalizedString>

#include <QGpgME/Protocol>
#include <QGpgME/QuickJob>

#include <QDateTime>

#include <gpgme++/key.h>

#include <gpgme.h>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;
using namespace GpgME;
using namespace QGpgME;

class AddSubkeyCommand::Private : public Command::Private
{
    AddSubkeyCommand *q_func() const
    {
        return static_cast<AddSubkeyCommand *>(q);
    }

public:
    explicit Private(AddSubkeyCommand *qq, KeyListController *c);
    ~Private() override;

    void slotDialogAccepted();
    void slotDialogRejected();
    void slotResult(const Error &err);

    void ensureDialogCreated();
    void createJob();
    void showErrorDialog(const Error &error);
    void showSuccessDialog();

    QPointer<AddSubkeyDialog> dialog;
    QPointer<QuickJob> job;
    QString algo;
};

AddSubkeyCommand::Private *AddSubkeyCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const AddSubkeyCommand::Private *AddSubkeyCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

AddSubkeyCommand::Private::Private(AddSubkeyCommand *qq, KeyListController *c)
    : Command::Private{qq, c}
{
}

AddSubkeyCommand::Private::~Private() = default;

void AddSubkeyCommand::Private::slotDialogAccepted()
{
    Q_ASSERT(dialog);

    createJob();
    QString usage;
    unsigned int flags = 0;
    if (dialog->usage().canEncrypt()) {
        flags |= GPGME_CREATE_ENCR;
        usage = QLatin1String("encr");
    } else if (dialog->usage().canSign()) {
        flags |= GPGME_CREATE_SIGN;
        usage = QLatin1String("sign");
    } else if (dialog->usage().canAuthenticate()) {
        flags |= GPGME_CREATE_AUTH;
        usage = QLatin1String("auth");
    }
    algo = dialog->algo();
    if (algo.startsWith(QLatin1String("curve"))) {
        if (dialog->usage().canEncrypt()) {
            algo.replace(QLatin1String("curve"), QLatin1String("cv"));
        } else {
            algo.replace(QLatin1String("curve"), QLatin1String("ed"));
        }
    } else if (algo != QLatin1String("default")) {
        algo = QLatin1String("%1/%2").arg(algo, usage);
    }
    job->startAddSubkey(key(), algo.toLatin1().data(), QDateTime(dialog->expires(), QTime()), flags);
}

void AddSubkeyCommand::Private::slotDialogRejected()
{
    Q_EMIT q->canceled();
    finished();
}

void AddSubkeyCommand::Private::slotResult(const Error &err)
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

void AddSubkeyCommand::Private::ensureDialogCreated()
{
    if (dialog) {
        return;
    }

    dialog = new AddSubkeyDialog{key()};
    applyWindowID(dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &QDialog::accepted, q, [this]() {
        slotDialogAccepted();
    });
    connect(dialog, &QDialog::rejected, q, [this]() {
        slotDialogRejected();
    });
}

void AddSubkeyCommand::Private::createJob()
{
    Q_ASSERT(!job);

    const auto backend = QGpgME::openpgp();
    if (!backend) {
        return;
    }

    const auto j = backend->quickJob();
    if (!j) {
        return;
    }

    connect(j, &QGpgME::Job::jobProgress, q, &Command::progress);
    connect(j, &QuickJob::result, q, [this](const auto &err) {
        slotResult(err);
    });

    job = j;
}

void AddSubkeyCommand::Private::showErrorDialog(const Error &err)
{
    error(
        i18n("<p>An error occurred while trying to add "
             "a new subkey to <b>%1</b>:</p><p>%2</p>",
             Formatting::formatForComboBox(key()),
             Formatting::errorAsString(err)));
}

void AddSubkeyCommand::Private::showSuccessDialog()
{
    success(i18n("Subkey added successfully."));
}

AddSubkeyCommand::AddSubkeyCommand(const GpgME::Key &key)
    : Command{key, new Private{this, nullptr}}
{
}

AddSubkeyCommand::~AddSubkeyCommand() = default;

void AddSubkeyCommand::doStart()
{
    d->ensureDialogCreated();
    Q_ASSERT(d->dialog);

    d->dialog->show();
}

void AddSubkeyCommand::doCancel()
{
    if (d->job) {
        d->job->slotCancel();
    }
}

#undef d
#undef q

#include "moc_addsubkeycommand.cpp"
