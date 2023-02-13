/* -*- mode: c++; c-basic-offset:4 -*-
    commands/setprimaryuseridcommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "setprimaryuseridcommand.h"

#include "command_p.h"

#include <KLocalizedString>

#if QGPGME_SUPPORTS_SET_PRIMARY_UID
#include <QGpgME/SetPrimaryUserIDJob>
#endif

#include <QGpgME/Protocol>
#include <gpgme++/key.h>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

class SetPrimaryUserIDCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::SetPrimaryUserIDCommand;
    SetPrimaryUserIDCommand *q_func() const
    {
        return static_cast<SetPrimaryUserIDCommand *>(q);
    }

public:
    explicit Private(SetPrimaryUserIDCommand *qq, const UserID &userId);
    ~Private() override;

    void startJob();

private:
    void createJob();
    void slotResult(const Error &err);
    void showErrorDialog(const Error &error);
    void showSuccessDialog();

private:
    GpgME::UserID userId;
#if QGPGME_SUPPORTS_SET_PRIMARY_UID
    QPointer<QGpgME::SetPrimaryUserIDJob> job;
#endif
};

SetPrimaryUserIDCommand::Private *SetPrimaryUserIDCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const SetPrimaryUserIDCommand::Private *SetPrimaryUserIDCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

SetPrimaryUserIDCommand::Private::Private(SetPrimaryUserIDCommand *qq, const UserID &userId)
    : Command::Private{qq}
    , userId{userId}
{
}

SetPrimaryUserIDCommand::Private::~Private() = default;

void Commands::SetPrimaryUserIDCommand::Private::startJob()
{
#if QGPGME_SUPPORTS_SET_PRIMARY_UID
    createJob();
    if (!job) {
        finished();
        return;
    }
    job->start(userId);
#else
    error(i18nc("@info", "The backend does not support this operation."));
#endif
}

void SetPrimaryUserIDCommand::Private::createJob()
{
#if QGPGME_SUPPORTS_SET_PRIMARY_UID
    Q_ASSERT(!job);

    const auto backend = QGpgME::openpgp();
    if (!backend) {
        return;
    }

    const auto j = backend->setPrimaryUserIDJob();
    if (!j) {
        return;
    }

#if QGPGME_JOB_HAS_NEW_PROGRESS_SIGNALS
    connect(j, &QGpgME::Job::jobProgress, q, &Command::progress);
#else
    connect(j, &QGpgME::Job::progress, q, [this](const QString &, int current, int total) {
        Q_EMIT q->progress(current, total);
    });
#endif
    connect(j, &QGpgME::SetPrimaryUserIDJob::result, q, [this](const GpgME::Error &err) {
        slotResult(err);
    });

    job = j;
#endif
}

void SetPrimaryUserIDCommand::Private::slotResult(const Error &err)
{
    if (err.isCanceled()) {
    } else if (err) {
        showErrorDialog(err);
    } else {
        showSuccessDialog();
    }
    finished();
}

void SetPrimaryUserIDCommand::Private::showErrorDialog(const Error &err)
{
    error(xi18nc("@info",
                 "<para>An error occurred while trying to flag the user ID<nl/><emphasis>%1</emphasis><nl/>as the primary user ID.</para>"
                 "<para><message>%2</message></para>",
                 QString::fromUtf8(userId.id()),
                 QString::fromLocal8Bit(err.asString())));
}

void SetPrimaryUserIDCommand::Private::showSuccessDialog()
{
    success(xi18nc("@info",
                   "<para>The user ID<nl/><emphasis>%1</emphasis><nl/>has been flagged successfully as the primary user ID.</para>",
                   QString::fromUtf8(userId.id())));
}

SetPrimaryUserIDCommand::SetPrimaryUserIDCommand(const GpgME::UserID &userId)
    : Command{new Private{this, userId}}
{
}

SetPrimaryUserIDCommand::~SetPrimaryUserIDCommand()
{
    qCDebug(KLEOPATRA_LOG).nospace() << this << "::" << __func__;
}

void SetPrimaryUserIDCommand::doStart()
{
    if (d->userId.isNull()) {
        d->finished();
        return;
    }

    const auto key = d->userId.parent();
    if (key.protocol() != GpgME::OpenPGP || !key.hasSecret()) {
        d->finished();
        return;
    }

    d->startJob();
}

void SetPrimaryUserIDCommand::doCancel()
{
    qCDebug(KLEOPATRA_LOG).nospace() << this << "::" << __func__;
#if QGPGME_SUPPORTS_SET_PRIMARY_UID
    if (d->job) {
        d->job->slotCancel();
    }
#endif
}

#undef d
#undef q
