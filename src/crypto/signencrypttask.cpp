/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/signencrypttask.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "signencrypttask.h"

#include <utils/input.h>
#include <utils/output.h>
#include <utils/path-helper.h>
#include <utils/kleo_assert.h>

#include <Libkleo/AuditLogEntry>
#include <Libkleo/Formatting>
#include <Libkleo/Stl_Util>
#include <Libkleo/KleoException>

#include <QGpgME/Protocol>
#include <QGpgME/SignJob>
#include <QGpgME/SignEncryptJob>
#include <QGpgME/EncryptJob>
#if QGPGME_SUPPORTS_ARCHIVE_JOBS
#include <QGpgME/EncryptArchiveJob>
#include <QGpgME/SignArchiveJob>
#include <QGpgME/SignEncryptArchiveJob>
#endif

#include <gpgme++/signingresult.h>
#include <gpgme++/encryptionresult.h>
#include <gpgme++/key.h>

#include <KLocalizedString>

#include "kleopatra_debug.h"
#include <QFileInfo>
#include <QPointer>
#include <QTextDocument> // for Qt::escape

using namespace Kleo;
using namespace Kleo::Crypto;
using namespace GpgME;

namespace
{

QString formatInputOutputLabel(const QString &input, const QString &output, bool outputDeleted)
{
    return i18nc("Input file --> Output file (rarr is arrow", "%1 &rarr; %2",
                 input.toHtmlEscaped(),
                 outputDeleted ? QStringLiteral("<s>%1</s>").arg(output.toHtmlEscaped()) : output.toHtmlEscaped());
}

class ErrorResult : public Task::Result
{
public:
    ErrorResult(bool sign, bool encrypt, const Error &err, const QString &errStr, const QString &input, const QString &output, const AuditLogEntry &auditLog)
        : Task::Result(), m_sign(sign), m_encrypt(encrypt), m_error(err), m_errString(errStr), m_inputLabel(input), m_outputLabel(output), m_auditLog(auditLog) {}

    QString overview() const override;
    QString details() const override;
    GpgME::Error error() const override
    {
        return m_error;
    }
    QString errorString() const override
    {
        return m_errString;
    }
    VisualCode code() const override
    {
        return NeutralError;
    }
    AuditLogEntry auditLog() const override
    {
        return m_auditLog;
    }
private:
    const bool m_sign;
    const bool m_encrypt;
    const Error m_error;
    const QString m_errString;
    const QString m_inputLabel;
    const QString m_outputLabel;
    const AuditLogEntry m_auditLog;
};

namespace
{
struct LabelAndError
{
    QString label;
    QString errorString;
};
}

class SignEncryptFilesResult : public Task::Result
{
public:
    SignEncryptFilesResult(const SigningResult &sr, const LabelAndError &input, const LabelAndError &output, bool outputCreated, const AuditLogEntry &auditLog)
        : Task::Result(),
          m_sresult(sr),
          m_input{input},
          m_output{output},
          m_outputCreated(outputCreated),
          m_auditLog(auditLog)
    {
        qCDebug(KLEOPATRA_LOG) << "\ninputError :" << m_input.errorString
                               << "\noutputError:" << m_output.errorString;
        Q_ASSERT(!m_sresult.isNull());
    }
    SignEncryptFilesResult(const EncryptionResult &er, const LabelAndError &input, const LabelAndError &output, bool outputCreated, const AuditLogEntry &auditLog)
        : Task::Result(),
          m_eresult(er),
          m_input{input},
          m_output{output},
          m_outputCreated(outputCreated),
          m_auditLog(auditLog)
    {
        qCDebug(KLEOPATRA_LOG) << "\ninputError :" << m_input.errorString
                               << "\noutputError:" << m_output.errorString;
        Q_ASSERT(!m_eresult.isNull());
    }
    SignEncryptFilesResult(const SigningResult &sr, const EncryptionResult &er, const LabelAndError &input, const LabelAndError &output, bool outputCreated,  const AuditLogEntry &auditLog)
        : Task::Result(),
          m_sresult(sr),
          m_eresult(er),
          m_input{input},
          m_output{output},
          m_outputCreated(outputCreated),
          m_auditLog(auditLog)
    {
        qCDebug(KLEOPATRA_LOG) << "\ninputError :" << m_input.errorString
                               << "\noutputError:" << m_output.errorString;
        Q_ASSERT(!m_sresult.isNull() || !m_eresult.isNull());
    }

    QString overview() const override;
    QString details() const override;
    GpgME::Error error() const override;
    QString errorString() const override;
    VisualCode code() const override;
    AuditLogEntry auditLog() const override;

private:
    const SigningResult m_sresult;
    const EncryptionResult m_eresult;
    const LabelAndError m_input;
    const LabelAndError m_output;
    const bool m_outputCreated;
    const AuditLogEntry m_auditLog;
};

static QString makeSigningOverview(const Error &err)
{
    if (err.isCanceled()) {
        return i18n("Signing canceled.");
    }

    if (err) {
        return i18n("Signing failed.");
    }
    return i18n("Signing succeeded.");
}

static QString makeResultOverview(const SigningResult &result)
{
    return makeSigningOverview(result.error());
}

static QString makeEncryptionOverview(const Error &err)
{
    if (err.isCanceled()) {
        return i18n("Encryption canceled.");
    }

    if (err) {
        return i18n("Encryption failed.");
    }

    return i18n("Encryption succeeded.");
}

static QString makeResultOverview(const EncryptionResult &result)
{
    return makeEncryptionOverview(result.error());
}

static QString makeResultOverview(const SigningResult &sr, const EncryptionResult &er)
{
    if (er.isNull() && sr.isNull()) {
        return QString();
    }
    if (er.isNull()) {
        return makeResultOverview(sr);
    }
    if (sr.isNull()) {
        return makeResultOverview(er);
    }
    if (sr.error().isCanceled() || sr.error()) {
        return makeResultOverview(sr);
    }
    if (er.error().isCanceled() || er.error()) {
        return makeResultOverview(er);
    }
    return i18n("Signing and encryption succeeded.");
}

static QString escape(QString s)
{
    s = s.toHtmlEscaped();
    s.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return s;
}

static QString makeResultDetails(const SigningResult &result, const QString &inputError, const QString &outputError)
{
    const Error err = result.error();
    if (err.code() == GPG_ERR_EIO) {
        if (!inputError.isEmpty()) {
            return i18n("Input error: %1", escape(inputError));
        } else if (!outputError.isEmpty()) {
            return i18n("Output error: %1", escape(outputError));
        }
    }

    if (err || err.isCanceled()) {
        return Formatting::errorAsString(err).toHtmlEscaped();
    }
    return QString();
}

static QString makeResultDetails(const EncryptionResult &result, const QString &inputError, const QString &outputError)
{
    const Error err = result.error();
    if (err.code() == GPG_ERR_EIO) {
        if (!inputError.isEmpty()) {
            return i18n("Input error: %1", escape(inputError));
        } else if (!outputError.isEmpty()) {
            return i18n("Output error: %1", escape(outputError));
        }
    }

    if (err || err.isCanceled()) {
        return Formatting::errorAsString(err).toHtmlEscaped();
    }
    return i18n(" Encryption succeeded.");
}

}

QString ErrorResult::overview() const
{
    Q_ASSERT(m_error || m_error.isCanceled());
    Q_ASSERT(m_sign || m_encrypt);
    const QString label = formatInputOutputLabel(m_inputLabel, m_outputLabel, true);
    const bool canceled = m_error.isCanceled();
    if (m_sign && m_encrypt) {
        return canceled ? i18n("%1: <b>Sign/encrypt canceled.</b>", label) : i18n(" %1: Sign/encrypt failed.", label);
    }
    return i18nc("label: result. Example: foo -> foo.gpg: Encryption failed.", "%1: <b>%2</b>", label,
                 m_sign ? makeSigningOverview(m_error) : makeEncryptionOverview(m_error));
}

QString ErrorResult::details() const
{
    return m_errString;
}

class SignEncryptTask::Private
{
    friend class ::Kleo::Crypto::SignEncryptTask;
    SignEncryptTask *const q;
public:
    explicit Private(SignEncryptTask *qq);

private:
    void startSignEncryptJob(GpgME::Protocol proto);
    std::unique_ptr<QGpgME::SignJob> createSignJob(GpgME::Protocol proto);
    std::unique_ptr<QGpgME::SignEncryptJob> createSignEncryptJob(GpgME::Protocol proto);
    std::unique_ptr<QGpgME::EncryptJob> createEncryptJob(GpgME::Protocol proto);

#if QGPGME_SUPPORTS_ARCHIVE_JOBS
    void startSignEncryptArchiveJob(GpgME::Protocol proto);
    std::unique_ptr<QGpgME::SignArchiveJob> createSignArchiveJob(GpgME::Protocol proto);
    std::unique_ptr<QGpgME::SignEncryptArchiveJob> createSignEncryptArchiveJob(GpgME::Protocol proto);
    std::unique_ptr<QGpgME::EncryptArchiveJob> createEncryptArchiveJob(GpgME::Protocol proto);
#endif

    std::shared_ptr<const Task::Result> makeErrorResult(const Error &err, const QString &errStr, const AuditLogEntry &auditLog);

private:
    void slotResult(const SigningResult &);
    void slotResult(const SigningResult &, const EncryptionResult &);
    void slotResult(const EncryptionResult &);

    void slotResult(const QGpgME::Job *, const SigningResult &, const EncryptionResult &);

private:
    std::shared_ptr<Input> input;
    std::shared_ptr<Output> output;
    QStringList inputFileNames;
    QString outputFileName;
    std::vector<Key> signers;
    std::vector<Key> recipients;

    bool sign     : 1;
    bool encrypt  : 1;
    bool detached : 1;
    bool symmetric: 1;
    bool clearsign: 1;
    bool archive  : 1;

    QPointer<QGpgME::Job> job;
    std::shared_ptr<OverwritePolicy> m_overwritePolicy;
};

SignEncryptTask::Private::Private(SignEncryptTask *qq)
    : q{qq}
    , sign{true}
    , encrypt{true}
    , detached{false}
    , clearsign{false}
    , archive{false}
    , m_overwritePolicy{new OverwritePolicy{nullptr}}
{
    q->setAsciiArmor(true);
}

std::shared_ptr<const Task::Result> SignEncryptTask::Private::makeErrorResult(const Error &err, const QString &errStr, const AuditLogEntry &auditLog)
{
    return std::shared_ptr<const ErrorResult>(new ErrorResult(sign, encrypt, err, errStr, q->label(), output ? output->label() : QString{}, auditLog));
}

SignEncryptTask::SignEncryptTask(QObject *p)
    : Task(p), d(new Private(this))
{

}

SignEncryptTask::~SignEncryptTask() {}

void SignEncryptTask::setInputFileName(const QString &fileName)
{
    kleo_assert(!d->job);
    kleo_assert(!fileName.isEmpty());
    d->inputFileNames = QStringList(fileName);
}

void SignEncryptTask::setInputFileNames(const QStringList &fileNames)
{
    kleo_assert(!d->job);
    kleo_assert(!fileNames.empty());
    d->inputFileNames = fileNames;
}

void SignEncryptTask::setInput(const std::shared_ptr<Input> &input)
{
    kleo_assert(!d->job);
    kleo_assert(input);
    d->input = input;
}

void SignEncryptTask::setOutput(const std::shared_ptr<Output> &output)
{
    kleo_assert(!d->job);
    kleo_assert(output);
    d->output = output;
}

void SignEncryptTask::setOutputFileName(const QString &fileName)
{
    kleo_assert(!d->job);
    kleo_assert(!fileName.isEmpty());
    d->outputFileName = fileName;
}

void SignEncryptTask::setSigners(const std::vector<Key> &signers)
{
    kleo_assert(!d->job);
    d->signers = signers;
}

void SignEncryptTask::setRecipients(const std::vector<Key> &recipients)
{
    kleo_assert(!d->job);
    d->recipients = recipients;
}

void SignEncryptTask::setOverwritePolicy(const std::shared_ptr<OverwritePolicy> &policy)
{
    kleo_assert(!d->job);
    d->m_overwritePolicy = policy;
}

void SignEncryptTask::setSign(bool sign)
{
    kleo_assert(!d->job);
    d->sign = sign;
}

void SignEncryptTask::setEncrypt(bool encrypt)
{
    kleo_assert(!d->job);
    d->encrypt = encrypt;
}

void SignEncryptTask::setDetachedSignature(bool detached)
{
    kleo_assert(!d->job);
    d->detached = detached;
}

void SignEncryptTask::setEncryptSymmetric(bool symmetric)
{
    kleo_assert(!d->job);
    d->symmetric = symmetric;
}

void SignEncryptTask::setClearsign(bool clearsign)
{
    kleo_assert(!d->job);
    d->clearsign = clearsign;
}

void SignEncryptTask::setCreateArchive(bool archive)
{
    kleo_assert(!d->job);
    d->archive = archive;
}

Protocol SignEncryptTask::protocol() const
{
    if (d->sign && !d->signers.empty()) {
        return d->signers.front().protocol();
    }
    if (d->encrypt || d->symmetric) {
        if (!d->recipients.empty()) {
            return d->recipients.front().protocol();
        } else {
            return GpgME::OpenPGP;    // symmetric OpenPGP encryption
        }
    }
    throw Kleo::Exception(gpg_error(GPG_ERR_INTERNAL),
                          i18n("Cannot determine protocol for task"));
}

QString SignEncryptTask::label() const
{
    if (d->input) {
        return d->input->label();
    } else if (!d->inputFileNames.empty()) {
        const auto firstFile = QFileInfo{d->inputFileNames.front()}.fileName();
        return d->inputFileNames.size() == 1 ? firstFile : i18nc("<name of first file>, ...", "%1, ...", firstFile);
    }
    return {};
}

QString SignEncryptTask::tag() const
{
    return Formatting::displayName(protocol());
}

unsigned long long SignEncryptTask::inputSize() const
{
    return d->input ? d->input->size() : 0U;
}

#if QGPGME_SUPPORTS_ARCHIVE_JOBS
static bool archiveJobsCanBeUsed(GpgME::Protocol protocol)
{
    return (protocol == GpgME::OpenPGP) && QGpgME::SignEncryptArchiveJob::isSupported();
}
#endif

void SignEncryptTask::doStart()
{
    kleo_assert(!d->job);
    if (d->sign) {
        kleo_assert(!d->signers.empty());
        if (d->archive) {
            kleo_assert(!d->detached && !d->clearsign);
        }
    }

    if (!d->output) {
        d->output = Output::createFromFile(d->outputFileName, d->m_overwritePolicy);
    }
    // release grab on policy, so that output can infer multiple outputs from use count > 1
    d->m_overwritePolicy.reset();

    const auto proto = protocol();
#if QGPGME_SUPPORTS_ARCHIVE_JOBS
    if (d->archive && archiveJobsCanBeUsed(proto)) {
        d->startSignEncryptArchiveJob(proto);
    } else
#endif
    {
        d->startSignEncryptJob(proto);
    }
}

void SignEncryptTask::Private::startSignEncryptJob(GpgME::Protocol proto)
{
    kleo_assert(input);

    if (encrypt || symmetric) {
        Context::EncryptionFlags flags = Context::AlwaysTrust;
        if (symmetric) {
            flags = static_cast<Context::EncryptionFlags>(flags | Context::Symmetric);
            qCDebug(KLEOPATRA_LOG) << "Adding symmetric flag";
        }
        if (sign) {
            std::unique_ptr<QGpgME::SignEncryptJob> job = createSignEncryptJob(proto);
            kleo_assert(job.get());
#if QGPGME_SUPPORTS_SET_FILENAME
            if (inputFileNames.size() == 1) {
                job->setFileName(inputFileNames.front());
            }
#endif

            job->start(signers, recipients,
                       input->ioDevice(), output->ioDevice(), flags);

            this->job = job.release();
        } else {
            std::unique_ptr<QGpgME::EncryptJob> job = createEncryptJob(proto);
            kleo_assert(job.get());
#if QGPGME_SUPPORTS_SET_FILENAME
            if (inputFileNames.size() == 1) {
                job->setFileName(inputFileNames.front());
            }
#endif

            job->start(recipients, input->ioDevice(), output->ioDevice(), flags);

            this->job = job.release();
        }
    } else if (sign) {
        std::unique_ptr<QGpgME::SignJob> job = createSignJob(proto);
        kleo_assert(job.get());
        kleo_assert(! (detached && clearsign));

        job->start(signers,
                   input->ioDevice(), output->ioDevice(),
                   detached ? GpgME::Detached : clearsign ?
                              GpgME::Clearsigned : GpgME::NormalSignatureMode);

        this->job = job.release();
    } else {
        kleo_assert(!"Either 'sign' or 'encrypt' or 'symmetric' must be set!");
    }
}

void SignEncryptTask::cancel()
{
    qCDebug(KLEOPATRA_LOG) << this << __func__;
    if (d->job) {
        d->job->slotCancel();
    }
}

std::unique_ptr<QGpgME::SignJob> SignEncryptTask::Private::createSignJob(GpgME::Protocol proto)
{
    const QGpgME::Protocol *const backend = (proto == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(backend);
    std::unique_ptr<QGpgME::SignJob> signJob(backend->signJob(q->asciiArmor(), /*textmode=*/false));
    kleo_assert(signJob.get());
#if QGPGME_JOB_HAS_NEW_PROGRESS_SIGNALS
    connect(signJob.get(), &QGpgME::Job::jobProgress, q, &SignEncryptTask::setProgress);
#else
    connect(signJob.get(), &QGpgME::Job::progress, q, [this](const QString &, int processed, int total) {
        q->setProgress(processed, total);
    });
#endif
    connect(signJob.get(), SIGNAL(result(GpgME::SigningResult,QByteArray)),
            q, SLOT(slotResult(GpgME::SigningResult)));
    return signJob;
}

std::unique_ptr<QGpgME::SignEncryptJob> SignEncryptTask::Private::createSignEncryptJob(GpgME::Protocol proto)
{
    const QGpgME::Protocol *const backend = (proto == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(backend);
    std::unique_ptr<QGpgME::SignEncryptJob> signEncryptJob(backend->signEncryptJob(q->asciiArmor(), /*textmode=*/false));
    kleo_assert(signEncryptJob.get());
#if QGPGME_JOB_HAS_NEW_PROGRESS_SIGNALS
    connect(signEncryptJob.get(), &QGpgME::Job::jobProgress, q, &SignEncryptTask::setProgress);
#else
    connect(signEncryptJob.get(), &QGpgME::Job::progress, q, [this](const QString &, int processed, int total) {
        q->setProgress(processed, total);
    });
#endif
    connect(signEncryptJob.get(), SIGNAL(result(GpgME::SigningResult,GpgME::EncryptionResult,QByteArray)),
            q, SLOT(slotResult(GpgME::SigningResult,GpgME::EncryptionResult)));
    return signEncryptJob;
}

std::unique_ptr<QGpgME::EncryptJob> SignEncryptTask::Private::createEncryptJob(GpgME::Protocol proto)
{
    const QGpgME::Protocol *const backend = (proto == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(backend);
    std::unique_ptr<QGpgME::EncryptJob> encryptJob(backend->encryptJob(q->asciiArmor(), /*textmode=*/false));
    kleo_assert(encryptJob.get());
#if QGPGME_JOB_HAS_NEW_PROGRESS_SIGNALS
    connect(encryptJob.get(), &QGpgME::Job::jobProgress, q, &SignEncryptTask::setProgress);
#else
    connect(encryptJob.get(), &QGpgME::Job::progress, q, [this](const QString &, int processed, int total) {
        q->setProgress(processed, total);
    });
#endif
    connect(encryptJob.get(), SIGNAL(result(GpgME::EncryptionResult,QByteArray)),
            q, SLOT(slotResult(GpgME::EncryptionResult)));
    return encryptJob;
}

#if QGPGME_SUPPORTS_ARCHIVE_JOBS
void SignEncryptTask::Private::startSignEncryptArchiveJob(GpgME::Protocol proto)
{
    kleo_assert(!input);

    const auto baseDirectory = heuristicBaseDirectory(inputFileNames);
    if (baseDirectory.isEmpty()) {
        throw Kleo::Exception(GPG_ERR_CONFLICT, i18n("Cannot find common base directory for these files:\n%1", inputFileNames.join(QLatin1Char('\n'))));
    }
    qCDebug(KLEOPATRA_LOG) << "heuristicBaseDirectory(" << inputFileNames << ") ->" << baseDirectory;
    const auto tempPaths = makeRelativeTo(baseDirectory, inputFileNames);
    const auto relativePaths = std::vector<QString>{tempPaths.begin(), tempPaths.end()};
    qCDebug(KLEOPATRA_LOG) << "relative paths:" << relativePaths;

    if (encrypt || symmetric) {
        Context::EncryptionFlags flags = Context::AlwaysTrust;
        if (symmetric) {
            flags = static_cast<Context::EncryptionFlags>(flags | Context::Symmetric);
            qCDebug(KLEOPATRA_LOG) << "Adding symmetric flag";
        }
        if (sign) {
            std::unique_ptr<QGpgME::SignEncryptArchiveJob> job = createSignEncryptArchiveJob(proto);
            kleo_assert(job.get());
            job->setBaseDirectory(baseDirectory);

            job->start(signers, recipients, relativePaths, output->ioDevice(), flags);

            this->job = job.release();
        } else {
            std::unique_ptr<QGpgME::EncryptArchiveJob> job = createEncryptArchiveJob(proto);
            kleo_assert(job.get());
            job->setBaseDirectory(baseDirectory);

            job->start(recipients, relativePaths, output->ioDevice(), flags);

            this->job = job.release();
        }
    } else if (sign) {
        std::unique_ptr<QGpgME::SignArchiveJob> job = createSignArchiveJob(proto);
        kleo_assert(job.get());
        job->setBaseDirectory(baseDirectory);

        job->start(signers, relativePaths, output->ioDevice());

        this->job = job.release();
    } else {
        kleo_assert(!"Either 'sign' or 'encrypt' or 'symmetric' must be set!");
    }
}

std::unique_ptr<QGpgME::SignArchiveJob> SignEncryptTask::Private::createSignArchiveJob(GpgME::Protocol proto)
{
    const QGpgME::Protocol *const backend = (proto == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(backend);
    std::unique_ptr<QGpgME::SignArchiveJob> signJob(backend->signArchiveJob(q->asciiArmor()));
    auto job = signJob.get();
    kleo_assert(job);
    connect(job, &QGpgME::SignArchiveJob::dataProgress, q, &SignEncryptTask::setProgress);
    connect(job, &QGpgME::SignArchiveJob::result, q, [this, job](const GpgME::SigningResult &signResult) {
        slotResult(job, signResult, EncryptionResult{});
    });
    return signJob;
}

std::unique_ptr<QGpgME::SignEncryptArchiveJob> SignEncryptTask::Private::createSignEncryptArchiveJob(GpgME::Protocol proto)
{
    const QGpgME::Protocol *const backend = (proto == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(backend);
    std::unique_ptr<QGpgME::SignEncryptArchiveJob> signEncryptJob(backend->signEncryptArchiveJob(q->asciiArmor()));
    auto job = signEncryptJob.get();
    kleo_assert(job);
    connect(job, &QGpgME::SignEncryptArchiveJob::dataProgress, q, &SignEncryptTask::setProgress);
    connect(job, &QGpgME::SignEncryptArchiveJob::result, q, [this, job](const GpgME::SigningResult &signResult, const GpgME::EncryptionResult &encryptResult) {
        slotResult(job, signResult, encryptResult);
    });
    return signEncryptJob;
}

std::unique_ptr<QGpgME::EncryptArchiveJob> SignEncryptTask::Private::createEncryptArchiveJob(GpgME::Protocol proto)
{
    const QGpgME::Protocol *const backend = (proto == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(backend);
    std::unique_ptr<QGpgME::EncryptArchiveJob> encryptJob(backend->encryptArchiveJob(q->asciiArmor()));
    auto job = encryptJob.get();
    kleo_assert(job);
    connect(job, &QGpgME::EncryptArchiveJob::dataProgress, q, &SignEncryptTask::setProgress);
    connect(job, &QGpgME::EncryptArchiveJob::result, q, [this, job](const GpgME::EncryptionResult &encryptResult) {
        slotResult(job, SigningResult{}, encryptResult);
    });
    return encryptJob;
}
#endif

void SignEncryptTask::Private::slotResult(const SigningResult &result)
{
    slotResult(qobject_cast<const QGpgME::Job *>(q->sender()), result, EncryptionResult{});
}

void SignEncryptTask::Private::slotResult(const SigningResult &sresult, const EncryptionResult &eresult)
{
    slotResult(qobject_cast<const QGpgME::Job *>(q->sender()), sresult, eresult);
}

void SignEncryptTask::Private::slotResult(const EncryptionResult &result)
{
    slotResult(qobject_cast<const QGpgME::Job *>(q->sender()), SigningResult{}, result);
}

void SignEncryptTask::Private::slotResult(const QGpgME::Job *job, const SigningResult &sresult, const EncryptionResult &eresult)
{
    const AuditLogEntry auditLog = AuditLogEntry::fromJob(job);
    bool outputCreated = false;
    if (input && input->failed()) {
        output->cancel();
        q->emitResult(makeErrorResult(Error::fromCode(GPG_ERR_EIO),
                                      i18n("Input error: %1", escape( input->errorString())),
                                      auditLog));
        return;
    } else if (sresult.error().code() || eresult.error().code()) {
        output->cancel();
    } else {
        try {
            kleo_assert(!sresult.isNull() || !eresult.isNull());
            output->finalize();
            outputCreated = true;
            if (input) {
                input->finalize();
            }
        } catch (const GpgME::Exception &e) {
            q->emitResult(makeErrorResult(e.error(), QString::fromLocal8Bit(e.what()), auditLog));
            return;
        }
    }

    const LabelAndError inputInfo{q->label(), input ? input->errorString() : QString{}};
    const LabelAndError outputInfo{output->label(), output->errorString()};
    q->emitResult(std::shared_ptr<Result>(new SignEncryptFilesResult(sresult, eresult, inputInfo, outputInfo, outputCreated, auditLog)));
}

QString SignEncryptFilesResult::overview() const
{
    const QString files = formatInputOutputLabel(m_input.label, m_output.label, !m_outputCreated);
    return files + QLatin1String(": ") + makeOverview(makeResultOverview(m_sresult, m_eresult));
}

QString SignEncryptFilesResult::details() const
{
    return errorString();
}

GpgME::Error SignEncryptFilesResult::error() const
{
    if (m_sresult.error().code()) {
        return m_sresult.error();
    }
    if (m_eresult.error().code()) {
        return m_eresult.error();
    }
    return {};
}

QString SignEncryptFilesResult::errorString() const
{
    const bool sign = !m_sresult.isNull();
    const bool encrypt = !m_eresult.isNull();

    kleo_assert(sign || encrypt);

    if (sign && encrypt) {
        return
            m_sresult.error().code() ? makeResultDetails(m_sresult, m_input.errorString, m_output.errorString) :
            m_eresult.error().code() ? makeResultDetails(m_eresult, m_input.errorString, m_output.errorString) :
            QString();
    }

    return
        sign   ? makeResultDetails(m_sresult, m_input.errorString, m_output.errorString) :
        /*else*/ makeResultDetails(m_eresult, m_input.errorString, m_output.errorString);
}

Task::Result::VisualCode SignEncryptFilesResult::code() const
{
    if (m_sresult.error().isCanceled() || m_eresult.error().isCanceled()) {
        return Warning;
    }
    return (m_sresult.error().code() || m_eresult.error().code()) ? NeutralError : NeutralSuccess;
}

AuditLogEntry SignEncryptFilesResult::auditLog() const
{
    return m_auditLog;
}

#include "moc_signencrypttask.cpp"
