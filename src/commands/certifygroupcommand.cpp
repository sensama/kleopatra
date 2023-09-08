/*
    commands/certifygroupcommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2023 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "certifygroupcommand.h"
#include "command_p.h"

#include <commands/exportopenpgpcertstoservercommand.h>
#include <dialogs/certifycertificatedialog.h>
#include <utils/keys.h>
#include <utils/tags.h>

#include <Libkleo/Algorithm>
#include <Libkleo/Formatting>
#include <Libkleo/KeyGroup>
#include <Libkleo/KeyHelpers>

#include <QGpgME/Protocol>
#include <QGpgME/SignKeyJob>

#include <QDate>

#include <gpgme++/key.h>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

namespace
{
struct CertificationResultData {
    std::vector<UserID> userIds;
    GpgME::Error error;
};
}

class CertifyGroupCommand::Private : public Command::Private
{
    friend class ::Kleo::CertifyGroupCommand;
    CertifyGroupCommand *q_func() const
    {
        return static_cast<CertifyGroupCommand *>(q);
    }

public:
    explicit Private(CertifyGroupCommand *qq);
    ~Private() override;

    void start();

private:
    void showDialog();
    void certifyCertificates();
    void startNextCertification();
    void createJob();
    void slotResult(const Error &err);
    void wrapUp();

private:
    KeyGroup group;
    std::vector<Key> certificates;
    QPointer<CertifyCertificateDialog> dialog;
    std::vector<UserID> userIdsToCertify;
    struct {
        Key certificationKey;
        QDate expirationDate;
        QString tags;
        bool exportable = false;
        bool sendToServer = false;
    } certificationOptions;
    struct {
        std::vector<UserID> userIds;
    } jobData;
    QPointer<QGpgME::SignKeyJob> job;
    std::vector<CertificationResultData> results;
};

CertifyGroupCommand::Private *CertifyGroupCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const CertifyGroupCommand::Private *CertifyGroupCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

CertifyGroupCommand::Private::Private(CertifyGroupCommand *qq)
    : Command::Private(qq)
{
}

CertifyGroupCommand::Private::~Private() = default;

void CertifyGroupCommand::Private::start()
{
    if (!group.isNull()) {
        const auto &groupKeys = group.keys();
        certificates = std::vector<GpgME::Key>(groupKeys.begin(), groupKeys.end());
    }
    if (certificates.empty()) {
        finished();
        return;
    }
    if (!allKeysHaveProtocol(certificates, GpgME::OpenPGP)) {
        const auto title = i18nc("@title:window", "Group Cannot Be Certified");
        const auto message = i18nc("@info", "This group contains S/MIME certificates which cannot be certified.");
        information(message, title);
        finished();
        return;
    }

    showDialog();
}

void CertifyGroupCommand::Private::showDialog()
{
    dialog = new CertifyCertificateDialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    applyWindowID(dialog);

    connect(dialog, &QDialog::accepted, q, [this]() {
        certifyCertificates();
    });
    connect(dialog, &QDialog::rejected, q, [this]() {
        canceled();
    });

    if (!group.isNull()) {
        dialog->setGroupName(group.name());
    }
    dialog->setCertificatesToCertify(certificates);
    dialog->show();
}

void CertifyGroupCommand::Private::certifyCertificates()
{
    userIdsToCertify = dialog->selectedUserIDs();
    if (userIdsToCertify.empty()) {
        canceled();
        return;
    }
    certificationOptions.certificationKey = dialog->selectedSecretKey();
    certificationOptions.expirationDate = dialog->expirationDate();
    certificationOptions.tags = dialog->tags();
    certificationOptions.exportable = dialog->exportableCertificationSelected();
    certificationOptions.sendToServer = dialog->sendToServer();

    startNextCertification();
}

void CertifyGroupCommand::Private::startNextCertification()
{
    Q_ASSERT(!userIdsToCertify.empty());

    const auto nextKey = userIdsToCertify.front().parent();
    // for now we only deal with primary user IDs
    jobData.userIds = {userIdsToCertify.front()};
    userIdsToCertify.erase(userIdsToCertify.begin());
    const std::vector<unsigned int> userIdIndexes = {0};

    createJob();
    job->setUserIDsToSign(userIdIndexes);
    if (const Error err = job->start(nextKey)) {
        QMetaObject::invokeMethod(
            q,
            [this, err]() {
                slotResult(err);
            },
            Qt::QueuedConnection);
    }
}

void CertifyGroupCommand::Private::createJob()
{
    Q_ASSERT(!job);

    std::unique_ptr<QGpgME::SignKeyJob> newJob{QGpgME::openpgp()->signKeyJob()};
    newJob->setDupeOk(true);
    newJob->setSigningKey(certificationOptions.certificationKey);
    newJob->setExportable(certificationOptions.exportable);
    if (!certificationOptions.tags.isEmpty()) {
        // do not set an empty remark to avoid an empty signature notation (GnuPG bug T5142)
        newJob->setRemark(certificationOptions.tags);
    }
    if (!certificationOptions.expirationDate.isNull()) {
        newJob->setExpirationDate(certificationOptions.expirationDate);
    }
    connect(newJob.get(), &QGpgME::SignKeyJob::result, q, [this](const GpgME::Error &result) {
        slotResult(result);
    });

    job = newJob.release();
}

void CertifyGroupCommand::Private::slotResult(const Error &err)
{
    results.push_back({
        jobData.userIds,
        err,
    });

    if (err.isCanceled()) {
        finished();
        return;
    }
    if (err) {
        // for now we only deal with primary user IDs
        Q_ASSERT(jobData.userIds.size() == 1);
        const Key key = jobData.userIds.front().parent();
        const QString message = i18nc("@info", "<p>Certifying the certificate <b>%1</b> failed.</p>", Formatting::formatForComboBox(key))
            + xi18nc("@info", "<para>Error: <message>%1</message></para>", Formatting::errorAsString(err));
        error(message);
    }

    if (!userIdsToCertify.empty()) {
        job.clear();
        jobData.userIds.clear();
        startNextCertification();
        return;
    }

    wrapUp();
}

static QString resultSummary(int successCount, int totalCount)
{
    if (successCount == totalCount) {
        return i18nc("@info", "All certificates were certified successfully.");
    }
    if (successCount == 0) {
        return i18nc("@info", "The certification of all certificates failed.");
    }
    return i18ncp("@info", //
                  "1 of %2 certificates was certified successfully.",
                  "%1 of %2 certificates were certified successfully.",
                  successCount,
                  totalCount);
}

void CertifyGroupCommand::Private::wrapUp()
{
    Q_ASSERT(userIdsToCertify.empty());

    const int successCount = Kleo::count_if(results, [](const auto &result) {
        return !result.error;
    });
    if (successCount == 0) {
        // there's no point in sending anything to the server
        certificationOptions.sendToServer = false;
    }
    const QString message = QLatin1String{"<p>"} + resultSummary(successCount, results.size()) + QLatin1String{"</p>"};
    if (successCount > 0) {
        information(message, i18nc("@title:window", "Certification Completed"));
    } else {
        error(message);
    }

    if (!certificationOptions.tags.isEmpty()) {
        Tags::enableTags();
    }
    finished();
}

CertifyGroupCommand::CertifyGroupCommand(const KeyGroup &group)
    : Command{new Private{this}}
{
    d->group = group;
}

CertifyGroupCommand::~CertifyGroupCommand() = default;

void CertifyGroupCommand::doStart()
{
    d->start();
}

void CertifyGroupCommand::doCancel()
{
    if (d->dialog) {
        d->dialog->close();
    }
    if (d->job) {
        d->job->slotCancel();
    }
}

#undef d
#undef q

#include "moc_certifygroupcommand.cpp"
