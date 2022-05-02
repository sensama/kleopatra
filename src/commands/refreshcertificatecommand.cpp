/* -*- mode: c++; c-basic-offset:4 -*-
    commands/refreshcertificatecommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "refreshcertificatecommand.h"
#include "command_p.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QGpgME/Protocol>
#ifdef QGPGME_SUPPORTS_KEY_REFRESH
#include <QGpgME/RefreshKeysJob>
#endif

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace GpgME;

class RefreshCertificateCommand::Private : public Command::Private
{
    friend class ::RefreshCertificateCommand;
    RefreshCertificateCommand *q_func() const
    {
        return static_cast<RefreshCertificateCommand *>(q);
    }
public:
    explicit Private(RefreshCertificateCommand *qq);
    ~Private() override;

    void start();
    void cancel();

#ifdef QGPGME_SUPPORTS_KEY_REFRESH
    std::unique_ptr<QGpgME::RefreshKeysJob> startJob();
#endif
    void onJobResult(const Error &err);
    void showError(const Error &err);

private:
    Key key;
#ifdef QGPGME_SUPPORTS_KEY_REFRESH
    QPointer<QGpgME::RefreshKeysJob> job;
#endif
};

RefreshCertificateCommand::Private *RefreshCertificateCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const RefreshCertificateCommand::Private *RefreshCertificateCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

RefreshCertificateCommand::Private::Private(RefreshCertificateCommand *qq)
    : Command::Private{qq}
{
}

RefreshCertificateCommand::Private::~Private() = default;

namespace
{
Key getKey(const std::vector<Key> &keys)
{
    if (keys.size() != 1) {
        qCWarning(KLEOPATRA_LOG) << "Expected exactly one key, but got" << keys.size();
        return {};
    }
    const Key key = keys.front();
    if (key.protocol() == GpgME::UnknownProtocol) {
        qCWarning(KLEOPATRA_LOG) << "Key has unknown protocol";
        return {};
    }
    return key;
}
}

void RefreshCertificateCommand::Private::start()
{
    key = getKey(keys());
    if (key.isNull()) {
        finished();
        return;
    }

#ifdef QGPGME_SUPPORTS_KEY_REFRESH
    auto refreshJob = startJob();
    if (!refreshJob) {
        finished();
        return;
    }
    job = refreshJob.release();
#else
    KMessageBox::error(parentWidgetOrView(), i18n("The backend does not support refreshing individual certificates."));
    finished();
#endif
}

void RefreshCertificateCommand::Private::cancel()
{
#ifdef QGPGME_SUPPORTS_KEY_REFRESH
    if (job) {
        job->slotCancel();
    }
    job.clear();
#endif
}

#ifdef QGPGME_SUPPORTS_KEY_REFRESH
std::unique_ptr<QGpgME::RefreshKeysJob> RefreshCertificateCommand::Private::startJob()
{
    auto jobFactory = key.protocol() == GpgME::OpenPGP ? QGpgME::openpgp() : QGpgME::smime();
    Q_ASSERT(jobFactory);
    std::unique_ptr<QGpgME::RefreshKeysJob> refreshJob{jobFactory->refreshKeysJob()};
    Q_ASSERT(refreshJob);

    connect(refreshJob.get(), &QGpgME::RefreshKeysJob::result,
            q, [this](const GpgME::Error &err) {
                onJobResult(err);
            });
    connect(refreshJob.get(), &QGpgME::Job::progress,
            q, &Command::progress);

    const GpgME::Error err = refreshJob->start({key});
    if (err) {
        showError(err);
        return {};
    }
    Q_EMIT q->info(i18nc("@info:status", "Refreshing key..."));

    return refreshJob;
}
#endif

void RefreshCertificateCommand::Private::onJobResult(const Error &err)
{
    if (err) {
        showError(err);
        finished();
        return;
    }

    if (!err.isCanceled()) {
        information(i18nc("@info", "The key was refreshed successfully."),
                    i18nc("@title:window", "Key Refreshed"));
    }
    finished();
}

void RefreshCertificateCommand::Private::showError(const Error &err)
{
    error(xi18nc("@info",
                 "<para>An error occurred while refreshing the key:</para>"
                 "<para><message>%1</message></para>",
                 QString::fromLocal8Bit(err.asString())),
          i18nc("@title:window", "Refreshing Failed"));
}

RefreshCertificateCommand::RefreshCertificateCommand(const GpgME::Key &key)
    : Command{key, new Private{this}}
{
}

RefreshCertificateCommand::~RefreshCertificateCommand() = default;

void RefreshCertificateCommand::doStart()
{
    d->start();
}

void RefreshCertificateCommand::doCancel()
{
    d->cancel();
}

#undef d
#undef q

#include "moc_refreshcertificatecommand.cpp"
