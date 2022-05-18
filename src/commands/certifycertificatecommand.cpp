/* -*- mode: c++; c-basic-offset:4 -*-
    commands/signcertificatecommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2019 g10code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "certifycertificatecommand.h"
#include "newcertificatecommand.h"

#include "command_p.h"

#include "exportopenpgpcertstoservercommand.h"
#include "dialogs/certifycertificatedialog.h"
#include "utils/tags.h"

#include <Libkleo/KeyCache>
#include <Libkleo/Formatting>

#include <QGpgME/Protocol>
#include <QGpgME/SignKeyJob>

#include <QDate>
#include <QEventLoop>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;
using namespace QGpgME;

class CertifyCertificateCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::CertifyCertificateCommand;
    CertifyCertificateCommand *q_func() const
    {
        return static_cast<CertifyCertificateCommand *>(q);
    }
public:
    explicit Private(CertifyCertificateCommand *qq, KeyListController *c);
    ~Private() override;

    void init();

private:
    void slotDialogRejected();
    void slotResult(const Error &err);
    void slotCertificationPrepared();

private:
    void ensureDialogCreated();
    void createJob();

private:
    std::vector<UserID> uids;
    QPointer<CertifyCertificateDialog> dialog;
    QPointer<QGpgME::SignKeyJob> job;
};

CertifyCertificateCommand::Private *CertifyCertificateCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const CertifyCertificateCommand::Private *CertifyCertificateCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

CertifyCertificateCommand::Private::Private(CertifyCertificateCommand *qq, KeyListController *c)
    : Command::Private(qq, c),
      uids(),
      dialog(),
      job()
{
}

CertifyCertificateCommand::Private::~Private()
{
    qCDebug(KLEOPATRA_LOG);
    if (dialog) {
        delete dialog;
        dialog = nullptr;
    }
}

CertifyCertificateCommand::CertifyCertificateCommand(KeyListController *c)
    : Command(new Private(this, c))
{
    d->init();
}

CertifyCertificateCommand::CertifyCertificateCommand(QAbstractItemView *v, KeyListController *c)
    : Command(v, new Private(this, c))
{
    d->init();
}

CertifyCertificateCommand::CertifyCertificateCommand(const GpgME::Key &key)
    : Command(key, new Private(this, nullptr))
{
    d->init();
}

CertifyCertificateCommand::CertifyCertificateCommand(const GpgME::UserID &uid)
    : Command(uid.parent(), new Private(this, nullptr))
{
    std::vector<UserID>(1, uid).swap(d->uids);
    d->init();
}

CertifyCertificateCommand::CertifyCertificateCommand(const std::vector<GpgME::UserID> &uids)
    : Command(uids.empty() ? Key() : uids.front().parent(), new Private(this, nullptr))
{
    d->uids = uids;
    d->init();
}

void CertifyCertificateCommand::Private::init()
{
}

CertifyCertificateCommand::~CertifyCertificateCommand()
{
    qCDebug(KLEOPATRA_LOG);
}

void CertifyCertificateCommand::doStart()
{

    const std::vector<Key> keys = d->keys();
    if (keys.size() != 1 ||
            keys.front().protocol() != GpgME::OpenPGP) {
        d->finished();
        return;
    }

    auto findAnyGoodKey = []() {
        const std::vector<Key> secKeys = KeyCache::instance()->secretKeys();
        return std::any_of(secKeys.cbegin(), secKeys.cend(), [](const Key &secKey) {
            return secKey.canCertify() && secKey.protocol() == OpenPGP && !secKey.isRevoked()
                   && !secKey.isExpired() && !secKey.isInvalid();
        });
    };

    if (!findAnyGoodKey()) {
        auto sel = KMessageBox::questionYesNo(d->parentWidgetOrView(),
                    xi18nc("@info", "To certify other certificates, you first need to create an OpenPGP certificate for yourself.") +
                    QStringLiteral("<br><br>") +
                    i18n("Do you wish to create one now?"),
                    i18n("Certification Not Possible"));
        if (sel == KMessageBox::Yes) {
            QEventLoop loop;
            auto cmd = new Commands::NewCertificateCommand();
            cmd->setParentWidget(d->parentWidgetOrView());
            cmd->setProtocol(GpgME::OpenPGP);
            connect(cmd, &Command::finished, &loop, &QEventLoop::quit);
            QMetaObject::invokeMethod(cmd, &Commands::NewCertificateCommand::start, Qt::QueuedConnection);
            loop.exec();
        } else {
            Q_EMIT(canceled());
            d->finished();
            return;
        }

        // Check again for secret keys
        if (!findAnyGoodKey()) {
            qCDebug(KLEOPATRA_LOG) << "Sec Keys still empty after keygen.";
            Q_EMIT(canceled());
            d->finished();
            return;
        }
    }

    const char *primary = keys.front().primaryFingerprint();
    const bool anyMismatch = std::any_of(d->uids.cbegin(), d->uids.cend(), [primary](const UserID &uid) {
        return qstricmp(uid.parent().primaryFingerprint(), primary) != 0;
    });
    if (anyMismatch) {
        qCWarning(KLEOPATRA_LOG) << "User ID <-> Key mismatch!";
        d->finished();
        return;
    }

    d->ensureDialogCreated();
    Q_ASSERT(d->dialog);

    Key target = d->key();
    if (!(target.keyListMode() & GpgME::SignatureNotations)) {
        target.update();
    }
    d->dialog->setCertificateToCertify(target);
    if (d->uids.size()) {
        d->dialog->setSelectedUserIDs(d->uids);
    }
    d->dialog->show();
}

void CertifyCertificateCommand::Private::slotDialogRejected()
{
    Q_EMIT q->canceled();
    finished();
}

void CertifyCertificateCommand::Private::slotResult(const Error &err)
{
    if (!err && !err.isCanceled() && dialog && dialog->exportableCertificationSelected() && dialog->sendToServer()) {
        auto const cmd = new ExportOpenPGPCertsToServerCommand(key());
        cmd->start();
    } else if (!err) {
        information(i18n("Certification successful."),
                    i18n("Certification Succeeded"));
    } else {
        error(i18n("<p>An error occurred while trying to certify<br/><br/>"
                   "<b>%1</b>:</p><p>\t%2</p>",
              Formatting::formatForComboBox(key()),
              QString::fromUtf8(err.asString())),
              i18n("Certification Error"));
    }
    if (!dialog->tags().isEmpty()) {
        Tags::enableTags();
    }

    finished();
}

void CertifyCertificateCommand::Private::slotCertificationPrepared()
{
    Q_ASSERT(dialog);

    createJob();
    Q_ASSERT(job);
    job->setExportable(dialog->exportableCertificationSelected());
    job->setNonRevocable(dialog->nonRevocableCertificationSelected());
    job->setUserIDsToSign(dialog->selectedUserIDs());
    job->setSigningKey(dialog->selectedSecretKey());
    job->setCheckLevel(dialog->selectedCheckLevel());
    if (!dialog->tags().isEmpty()) {
        // do not set an empty remark to avoid an empty signature notation (GnuPG bug T5142)
        job->setRemark(dialog->tags());
    }
    job->setDupeOk(true);
    if (dialog->trustSignatureSelected() && !dialog->trustSignatureDomain().isEmpty()) {
        // always create level 1 trust signatures with complete trust
        job->setTrustSignature(TrustSignatureTrust::Complete, 1, dialog->trustSignatureDomain());
    }
    if (!dialog->expirationDate().isNull()) {
        job->setExpirationDate(dialog->expirationDate());
    }

    if (const Error err = job->start(key())) {
        slotResult(err);
    }
}

void CertifyCertificateCommand::doCancel()
{
    qCDebug(KLEOPATRA_LOG);
    if (d->job) {
        d->job->slotCancel();
    }
}

void CertifyCertificateCommand::Private::ensureDialogCreated()
{
    if (dialog) {
        return;
    }

    dialog = new CertifyCertificateDialog;
    applyWindowID(dialog);

    connect(dialog, &QDialog::rejected, q, [this]() { slotDialogRejected(); });
    connect(dialog, &QDialog::accepted, q, [this]() { slotCertificationPrepared(); });
}

void CertifyCertificateCommand::Private::createJob()
{
    Q_ASSERT(!job);

    Q_ASSERT(key().protocol() == OpenPGP);
    const auto backend = QGpgME::openpgp();
    if (!backend) {
        return;
    }

    SignKeyJob *const j = backend->signKeyJob();
    if (!j) {
        return;
    }

    connect(j, &Job::progress,
            q, &Command::progress);
    connect(j, &SignKeyJob::result, q, [this](const GpgME::Error &result) { slotResult(result); });

    job = j;
}

#undef d
#undef q

#include "moc_certifycertificatecommand.cpp"
