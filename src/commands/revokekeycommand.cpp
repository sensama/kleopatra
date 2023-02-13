/* -*- mode: c++; c-basic-offset:4 -*-
    commands/revokekeycommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "revokekeycommand.h"
#include "command_p.h"
#include "dialogs/revokekeydialog.h"

#include <Libkleo/Formatting>

#include <KLocalizedString>

#if QGPGME_SUPPORTS_KEY_REVOCATION
#include <QGpgME/RevokeKeyJob>
#endif

#include "kleopatra_debug.h"
#include <QGpgME/Protocol>

using namespace Kleo;
using namespace GpgME;

class RevokeKeyCommand::Private : public Command::Private
{
    friend class ::RevokeKeyCommand;
    RevokeKeyCommand *q_func() const
    {
        return static_cast<RevokeKeyCommand *>(q);
    }
public:
    explicit Private(RevokeKeyCommand *qq, KeyListController *c = nullptr);
    ~Private() override;

    void start();
    void cancel();

private:
    void ensureDialogCreated();
    void onDialogAccepted();
    void onDialogRejected();

#if QGPGME_SUPPORTS_KEY_REVOCATION
    std::unique_ptr<QGpgME::RevokeKeyJob> startJob();
#endif
    void onJobResult(const Error &err);
    void showError(const Error &err);

private:
    Key key;
    QPointer<RevokeKeyDialog> dialog;
#if QGPGME_SUPPORTS_KEY_REVOCATION
    QPointer<QGpgME::RevokeKeyJob> job;
#endif
};

RevokeKeyCommand::Private *RevokeKeyCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const RevokeKeyCommand::Private *RevokeKeyCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

RevokeKeyCommand::Private::Private(RevokeKeyCommand *qq, KeyListController *c)
    : Command::Private{qq, c}
{
}

RevokeKeyCommand::Private::~Private() = default;

namespace
{
Key getKey(const std::vector<Key> &keys)
{
    if (keys.size() != 1) {
        qCWarning(KLEOPATRA_LOG) << "Expected exactly one key, but got" << keys.size();
        return {};
    }
    const Key key = keys.front();
    if (key.protocol() != GpgME::OpenPGP) {
        qCWarning(KLEOPATRA_LOG) << "Expected OpenPGP key, but got" << Formatting::displayName(key.protocol()) << "key";
        return {};
    }
    return key;
}
}

void RevokeKeyCommand::Private::start()
{
    key = getKey(keys());
    if (key.isNull()) {
        finished();
        return;
    }

    if (key.isRevoked()) {
        information(i18nc("@info", "This key has already been revoked."));
        finished();
        return;
    }

    ensureDialogCreated();
    Q_ASSERT(dialog);

    dialog->setKey(key);
    dialog->show();
}

void RevokeKeyCommand::Private::cancel()
{
#if QGPGME_SUPPORTS_KEY_REVOCATION
    if (job) {
        job->slotCancel();
    }
    job.clear();
#endif
}

void RevokeKeyCommand::Private::ensureDialogCreated()
{
    if (dialog) {
        return;
    }

    dialog = new RevokeKeyDialog;
    applyWindowID(dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &QDialog::accepted, q, [this]() { onDialogAccepted(); });
    connect(dialog, &QDialog::rejected, q, [this]() { onDialogRejected(); });
}

void RevokeKeyCommand::Private::onDialogAccepted()
{
#if QGPGME_SUPPORTS_KEY_REVOCATION
    auto revokeJob = startJob();
    if (!revokeJob) {
        finished();
        return;
    }
    job = revokeJob.release();
#endif
}

void RevokeKeyCommand::Private::onDialogRejected()
{
    canceled();
}

namespace
{
std::vector<std::string> toStdStrings(const QStringList &l)
{
    std::vector<std::string> v;
    v.reserve(l.size());
    std::transform(std::begin(l), std::end(l),
                   std::back_inserter(v),
                   std::mem_fn(&QString::toStdString));
    return v;
}

auto descriptionToLines(const QString &description)
{
    std::vector<std::string> lines;
    if (!description.isEmpty()) {
        lines = toStdStrings(description.split(QLatin1Char('\n')));
    }
    return lines;
}
}

#if QGPGME_SUPPORTS_KEY_REVOCATION
std::unique_ptr<QGpgME::RevokeKeyJob> RevokeKeyCommand::Private::startJob()
{
    std::unique_ptr<QGpgME::RevokeKeyJob> revokeJob{QGpgME::openpgp()->revokeKeyJob()};
    Q_ASSERT(revokeJob);

    connect(revokeJob.get(), &QGpgME::RevokeKeyJob::result,
            q, [this](const GpgME::Error &err) {
                onJobResult(err);
            });
#if QGPGME_JOB_HAS_NEW_PROGRESS_SIGNALS
    connect(revokeJob.get(), &QGpgME::Job::jobProgress,
            q, &Command::progress);
#else
    connect(revokeJob.get(), &QGpgME::Job::progress,
            q, [this](const QString &, int current, int total) { Q_EMIT q->progress(current, total); });
#endif

    const auto description = descriptionToLines(dialog->description());
    const GpgME::Error err = revokeJob->start(key, dialog->reason(), description);
    if (err) {
        showError(err);
        return {};
    }
    Q_EMIT q->info(i18nc("@info:status", "Revoking key..."));

    return revokeJob;
}
#endif

void RevokeKeyCommand::Private::onJobResult(const Error &err)
{
    if (err) {
        showError(err);
        finished();
        return;
    }

    if (!err.isCanceled()) {
        information(i18nc("@info", "The key was revoked successfully."),
                    i18nc("@title:window", "Key Revoked"));
    }
    finished();
}

void RevokeKeyCommand::Private::showError(const Error &err)
{
    error(xi18nc("@info",
                 "<para>An error occurred during the revocation:</para>"
                 "<para><message>%1</message></para>",
                 QString::fromLocal8Bit(err.asString())),
          i18nc("@title:window", "Revocation Failed"));
}

RevokeKeyCommand::RevokeKeyCommand(QAbstractItemView *v, KeyListController *c)
    : Command{v, new Private{this, c}}
{
}

RevokeKeyCommand::RevokeKeyCommand(const GpgME::Key &key)
    : Command{key, new Private{this}}
{
}

RevokeKeyCommand::~RevokeKeyCommand() = default;

void RevokeKeyCommand::doStart()
{
    d->start();
}

void RevokeKeyCommand::doCancel()
{
    d->cancel();
}

#undef d
#undef q

#include "moc_revokekeycommand.cpp"
