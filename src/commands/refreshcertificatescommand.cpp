/* -*- mode: c++; c-basic-offset:4 -*-
    commands/refreshcertificatecommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "command_p.h"
#include "refreshcertificatescommand.h"
#include <settings.h>

#include <Libkleo/Formatting>
#include <Libkleo/GnuPG>
#include <Libkleo/KeyHelpers>

#include <KLocalizedString>
#include <KMessageBox>

#include <QGpgME/Protocol>
#include <QGpgME/ReceiveKeysJob>
#include <QGpgME/RefreshKeysJob>
#if QGPGME_SUPPORTS_WKD_REFRESH_JOB
#include <QGpgME/WKDRefreshJob>
#endif

#include <gpgme++/importresult.h>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace GpgME;

class RefreshCertificatesCommand::Private : public Command::Private
{
    friend class ::RefreshCertificatesCommand;
    RefreshCertificatesCommand *q_func() const
    {
        return static_cast<RefreshCertificatesCommand *>(q);
    }

public:
    explicit Private(RefreshCertificatesCommand *qq);
    explicit Private(RefreshCertificatesCommand *qq, KeyListController *c);
    ~Private() override;

    void start();
    void cancel();

    std::unique_ptr<QGpgME::ReceiveKeysJob> startKeyserverJob();
    std::unique_ptr<QGpgME::RefreshKeysJob> startSMIMEJob();
#if QGPGME_SUPPORTS_WKD_REFRESH_JOB
    std::unique_ptr<QGpgME::WKDRefreshJob> startWKDRefreshJob();
#endif

    void onKeyserverJobResult(const ImportResult &result);
    void onWKDRefreshJobResult(const ImportResult &result);
    void onSMIMEJobResult(const Error &err);

    void checkFinished();

private:
    QPointer<QGpgME::Job> pgpJob;
    QPointer<QGpgME::Job> smimeJob;
    QPointer<QGpgME::Job> wkdJob;

    std::vector<Key> pgpKeys;
    std::vector<Key> smimeKeys;
    std::vector<Key> wkdKeys;

    ImportResult keyserverResult;
    ImportResult wkdRefreshResult;
    std::optional<Error> smimeError;
};

RefreshCertificatesCommand::Private *RefreshCertificatesCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const RefreshCertificatesCommand::Private *RefreshCertificatesCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

RefreshCertificatesCommand::Private::Private(RefreshCertificatesCommand *qq)
    : Command::Private{qq}
{
}

RefreshCertificatesCommand::Private::Private(RefreshCertificatesCommand *qq, KeyListController *c)
    : Command::Private{qq, c}
{
}

RefreshCertificatesCommand::Private::~Private() = default;

void RefreshCertificatesCommand::Private::start()
{
    if (std::ranges::any_of(keys(), [](const auto &key) {
            return key.protocol() == GpgME::UnknownProtocol;
        })) {
        qCWarning(KLEOPATRA_LOG) << "Key has unknown protocol";
        finished();
        return;
    }

    std::unique_ptr<QGpgME::Job> pgpRefreshJob;
    std::unique_ptr<QGpgME::Job> smimeRefreshJob;
    std::unique_ptr<QGpgME::Job> wkdRefreshJob;

    auto keysByProtocol = Kleo::partitionKeysByProtocol(keys());

    pgpKeys = keysByProtocol.openpgp;
    smimeKeys = keysByProtocol.cms;

    if (!smimeKeys.empty()) {
        smimeRefreshJob = startSMIMEJob();
    }

    if (!pgpKeys.empty()) {
        if (haveKeyserverConfigured()) {
            pgpRefreshJob = startKeyserverJob();
        } else {
            keyserverResult = ImportResult{Error::fromCode(GPG_ERR_USER_1)};
        }
#if QGPGME_SUPPORTS_WKD_REFRESH_JOB
        wkdRefreshJob = startWKDRefreshJob();
#endif
    }

    if (!pgpRefreshJob && !smimeRefreshJob && !wkdRefreshJob) {
        finished();
        return;
    }
    pgpJob = pgpRefreshJob.release();
    smimeJob = smimeRefreshJob.release();
    wkdJob = wkdRefreshJob.release();
}

void RefreshCertificatesCommand::Private::cancel()
{
    if (pgpJob) {
        pgpJob->slotCancel();
    }

    if (smimeJob) {
        smimeJob->slotCancel();
    }

    if (wkdJob) {
        wkdJob->slotCancel();
    }

    pgpJob.clear();
    smimeJob.clear();
    wkdJob.clear();

    smimeError = Error::fromCode(GPG_ERR_CANCELED);
}

std::unique_ptr<QGpgME::ReceiveKeysJob> RefreshCertificatesCommand::Private::startKeyserverJob()
{
    std::unique_ptr<QGpgME::ReceiveKeysJob> refreshJob{QGpgME::openpgp()->receiveKeysJob()};
    Q_ASSERT(refreshJob);

    connect(refreshJob.get(), &QGpgME::ReceiveKeysJob::result, q, [this](const GpgME::ImportResult &result) {
        onKeyserverJobResult(result);
    });
    connect(refreshJob.get(), &QGpgME::Job::jobProgress, q, &Command::progress);

    refreshJob->start(Kleo::getFingerprints(pgpKeys));

    Q_EMIT q->info(i18nc("@info:status", "Updating key..."));

    return refreshJob;
}

std::unique_ptr<QGpgME::RefreshKeysJob> RefreshCertificatesCommand::Private::startSMIMEJob()
{
    std::unique_ptr<QGpgME::RefreshKeysJob> refreshJob{QGpgME::smime()->refreshKeysJob()};
    Q_ASSERT(refreshJob);

    connect(refreshJob.get(), &QGpgME::RefreshKeysJob::result, q, [this](const GpgME::Error &err) {
        onSMIMEJobResult(err);
    });
    connect(refreshJob.get(), &QGpgME::Job::jobProgress, q, &Command::progress);

    refreshJob->start(smimeKeys);

    Q_EMIT q->info(i18nc("@info:status", "Updating certificate..."));

    return refreshJob;
}

#if QGPGME_SUPPORTS_WKD_REFRESH_JOB
std::unique_ptr<QGpgME::WKDRefreshJob> RefreshCertificatesCommand::Private::startWKDRefreshJob()
{
    for (const auto &key : pgpKeys) {
        const auto userIds = key.userIDs();
        if (std::any_of(userIds.begin(), userIds.end(), [](const auto &userId) {
                return !userId.isRevoked() && !userId.addrSpec().empty() && userId.origin() == Key::OriginWKD;
            })) {
            wkdKeys.push_back(key);
        }
    }

    std::unique_ptr<QGpgME::WKDRefreshJob> refreshJob{QGpgME::openpgp()->wkdRefreshJob()};
    Q_ASSERT(refreshJob);

    connect(refreshJob.get(), &QGpgME::WKDRefreshJob::result, q, [this](const GpgME::ImportResult &result) {
        onWKDRefreshJobResult(result);
    });
    connect(refreshJob.get(), &QGpgME::Job::jobProgress, q, &Command::progress);
    Error err;

    if (!Settings{}.queryWKDsForAllUserIDs()) {
        // check if key is eligible for WKD refresh, i.e. if any user ID has WKD as origin
        if (wkdKeys.empty()) {
            wkdRefreshResult = ImportResult{Error::fromCode(GPG_ERR_USER_1)};
            QMetaObject::invokeMethod(
                q,
                [this]() {
                    checkFinished();
                },
                Qt::QueuedConnection);
            return {};
        }
        err = refreshJob->start(wkdKeys);
    } else {
        std::vector<UserID> userIds;
        wkdKeys = pgpKeys;
        for (const auto &key : pgpKeys) {
            const auto newUserIds = key.userIDs();
            userIds.insert(userIds.end(), newUserIds.begin(), newUserIds.end());
        }
        err = refreshJob->start(userIds);
    }

    Q_EMIT q->info(i18nc("@info:status", "Updating key..."));

    return refreshJob;
}
#endif

namespace
{
static auto informationOnChanges(const ImportResult &result)
{
    QString text;

    // if additional keys have been retrieved via WKD, then most of the below
    // details are just a guess and may concern the additional keys instead of
    // the refresh keys; this could only be clarified by a thorough comparison of
    // unrefreshed and refreshed key

    if (result.numUnchanged() == result.numConsidered()) {
        // if numUnchanged < numConsidered, then it is not clear whether the refreshed key
        // hasn't changed or whether another key retrieved via WKD hasn't changed
        text = i18n("The certificate has not changed.");
    } else if (result.newRevocations() > 0) {
        // it is possible that a revoked key has been newly imported via WKD,
        // but it is much more likely that the refreshed key was revoked
        text = i18n("The certificate has been revoked.");
    } else {
        // it doesn't make much sense to list below details if the key has been revoked
        text = i18n("The certificate has been updated.");

        QStringList details;
        if (result.newUserIDs() > 0) {
            details.push_back(i18n("New user IDs: %1", result.newUserIDs()));
        }
        if (result.newSubkeys() > 0) {
            details.push_back(i18n("New subkeys: %1", result.newSubkeys()));
        }
        if (result.newSignatures() > 0) {
            details.push_back(i18n("New signatures: %1", result.newSignatures()));
        }
        if (!details.empty()) {
            text += QLatin1String{"<br><br>"} + details.join(QLatin1String{"<br>"});
        }
    }

    text = QLatin1String{"<p>"} + text + QLatin1String{"</p>"};
    if (result.numImported() > 0) {
        text += QLatin1String{"<p>"}
            + i18np("Additionally, one new key has been retrieved.", "Additionally, %1 new keys have been retrieved.", result.numImported())
            + QLatin1String{"</p>"};
    }

    return text;
}

}

void RefreshCertificatesCommand::Private::onKeyserverJobResult(const ImportResult &result)
{
    keyserverResult = result;

    if (result.error().isCanceled()) {
        pgpJob.clear();
        finished();
        return;
    }

    pgpJob.clear();
    checkFinished();
}

void RefreshCertificatesCommand::Private::onWKDRefreshJobResult(const ImportResult &result)
{
    wkdRefreshResult = result;

    if (result.error().isCanceled()) {
        pgpJob.clear();
        finished();
        return;
    }

    wkdJob.clear();
    checkFinished();
}

void RefreshCertificatesCommand::Private::onSMIMEJobResult(const Error &error)
{
    smimeError = error;

    if (error.isCanceled()) {
        smimeJob.clear();
        finished();
        return;
    }

    smimeJob.clear();
    checkFinished();
}

RefreshCertificatesCommand::RefreshCertificatesCommand(QAbstractItemView *v, KeyListController *p)
    : Command(v, new Private(this, p))
{
}

RefreshCertificatesCommand::RefreshCertificatesCommand(const Key &key)
    : Command(key, new Private(this))
{
}

RefreshCertificatesCommand::~RefreshCertificatesCommand() = default;

void RefreshCertificatesCommand::doStart()
{
    d->start();
}

void RefreshCertificatesCommand::doCancel()
{
    d->cancel();
}

void RefreshCertificatesCommand::Private::checkFinished()
{
    if (smimeJob || pgpJob || wkdJob) {
        return;
    }

    if (smimeError && smimeError->code() == GPG_ERR_CANCELED) {
        finished();
        return;
    }

    const auto pgpSkipped = keyserverResult.error().code() == GPG_ERR_USER_1;
    const auto pgpKeyNotFound = keyserverResult.error().code() == GPG_ERR_NO_DATA;
    const auto wkdSkipped = wkdRefreshResult.error().code() == GPG_ERR_USER_1;

    const auto hasSmimeError = smimeError && *smimeError;
    const auto hasPgpError = !keyserverResult.isNull() && keyserverResult.error() && !pgpSkipped && !pgpKeyNotFound;
    const auto hasWkdError = !wkdRefreshResult.isNull() && wkdRefreshResult.error() && !wkdSkipped;

    bool success = false;
    QString text;

    if (!pgpKeys.empty()) {
        text += QLatin1String{"<p><strong>"} + i18nc("@info", "Result of OpenPGP certificate update from keyserver, LDAP server, or Active Directory")
            + QLatin1String{"</strong></p>"};
        if (hasPgpError) {
            text += xi18nc("@info", "<para>Update failed:</para><para><message>%1</message></para>", Formatting::errorAsString(keyserverResult.error()));
        } else if (pgpSkipped) {
            text += xi18nc("@info", "<para>Update skipped because no OpenPGP keyserver is configured.</para>");
        } else if (pgpKeyNotFound && pgpKeys.size() == 1) {
            text += xi18nc("@info", "<para>The certificate was not found.</para>");
        } else if (pgpKeys.size() > 1) {
            success = true;
            text += xi18ncp("@info", "<para>The certificate was updated.</para>", "<para>The certificates were updated.</para>", pgpKeys.size());
        } else if (pgpKeys.size() == 1) {
            success = true;
            text += informationOnChanges(keyserverResult);
        }
    }

    if (!wkdKeys.empty() && !wkdSkipped) {
        text += QLatin1String{"<p><strong>"} + i18nc("@info", "Result of update from Web Key Directory") + QLatin1String{"</strong></p>"};
        if (hasWkdError) {
            text += xi18nc("@info", "<para>Update failed:</para><para><message>%1</message></para>", Formatting::errorAsString(wkdRefreshResult.error()));
        } else {
            success = true;
            text += xi18ncp("@info", "<para>The certificate was updated.</para>", "<para>The certificates were updated.</para>", pgpKeys.size());
        }
    }

    if (!smimeKeys.empty()) {
        text += QLatin1String{"<p><strong>"} + i18nc("@info", "Result of S/MIME certificate update") + QLatin1String{"</strong></p>"};
        if (hasSmimeError) {
            text += xi18nc("@info", "<para>Update failed:</para><para><message>%1</message></para>", Formatting::errorAsString(*smimeError));
        } else {
            success = true;
            text += xi18ncp("@info", "<para>The certificate was updated.</para>", "<para>The certificates were updated.</para>", smimeKeys.size());
        }
    }

    information(text,
                success ? i18ncp("@title:window", "Certificate Updated", "Certificates Updated", keys().size()) : i18nc("@title:window", "Update Failed"));
    finished();
}

#undef d
#undef q

#include "moc_refreshcertificatescommand.cpp"
