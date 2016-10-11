/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/signencryptfilestask.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config-kleopatra.h>

#include "signencryptfilestask.h"

#include <utils/input.h>
#include <utils/output.h>
#include <utils/path-helper.h>
#include <utils/kleo_assert.h>
#include <utils/auditlog.h>

#include <Libkleo/Formatting>
#include <Libkleo/Stl_Util>
#include <Libkleo/Exception>

#include <QGpgME/Protocol>
#include <QGpgME/SignJob>
#include <QGpgME/SignEncryptJob>
#include <QGpgME/EncryptJob>

#include <gpgme++/signingresult.h>
#include <gpgme++/encryptionresult.h>
#include <gpgme++/key.h>

#include <KLocalizedString>

#include "kleopatra_debug.h"
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
    ErrorResult(bool sign, bool encrypt, const Error &err, const QString &errStr, const QString &input, const QString &output, const AuditLog &auditLog)
        : Task::Result(), m_sign(sign), m_encrypt(encrypt), m_error(err), m_errString(errStr), m_inputLabel(input), m_outputLabel(output), m_auditLog(auditLog) {}

    QString overview() const Q_DECL_OVERRIDE;
    QString details() const Q_DECL_OVERRIDE;
    int errorCode() const Q_DECL_OVERRIDE
    {
        return m_error.encodedError();
    }
    QString errorString() const Q_DECL_OVERRIDE
    {
        return m_errString;
    }
    VisualCode code() const Q_DECL_OVERRIDE
    {
        return NeutralError;
    }
    AuditLog auditLog() const Q_DECL_OVERRIDE
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
    const AuditLog m_auditLog;
};

class SignEncryptFilesResult : public Task::Result
{
public:
    SignEncryptFilesResult(const SigningResult &sr, const std::shared_ptr<Input> &input, const std::shared_ptr<Output> &output, bool outputCreated, const AuditLog &auditLog)
        : Task::Result(),
          m_sresult(sr),
          m_inputLabel(input ? input->label() : QString()),
          m_inputErrorString(input ? input->errorString() : QString()),
          m_outputLabel(output ? output->label() : QString()),
          m_outputErrorString(output ? output->errorString() : QString()),
          m_outputCreated(outputCreated),
          m_auditLog(auditLog)
    {
        qCDebug(KLEOPATRA_LOG) << endl
                               << "inputError :" << m_inputErrorString << endl
                               << "outputError:" << m_outputErrorString;
        assert(!m_sresult.isNull());
    }
    SignEncryptFilesResult(const EncryptionResult &er, const std::shared_ptr<Input> &input, const std::shared_ptr<Output> &output, bool outputCreated, const AuditLog &auditLog)
        : Task::Result(),
          m_eresult(er),
          m_inputLabel(input ? input->label() : QString()),
          m_inputErrorString(input ? input->errorString() : QString()),
          m_outputLabel(output ? output->label() : QString()),
          m_outputErrorString(output ? output->errorString() : QString()),
          m_outputCreated(outputCreated),
          m_auditLog(auditLog)
    {
        qCDebug(KLEOPATRA_LOG) << endl
                               << "inputError :" << m_inputErrorString << endl
                               << "outputError:" << m_outputErrorString;
        assert(!m_eresult.isNull());
    }
    SignEncryptFilesResult(const SigningResult &sr, const EncryptionResult &er, const std::shared_ptr<Input> &input, const std::shared_ptr<Output> &output, bool outputCreated,  const AuditLog &auditLog)
        : Task::Result(),
          m_sresult(sr),
          m_eresult(er),
          m_inputLabel(input ? input->label() : QString()),
          m_inputErrorString(input ? input->errorString() : QString()),
          m_outputLabel(output ? output->label() : QString()),
          m_outputErrorString(output ? output->errorString() : QString()),
          m_outputCreated(outputCreated),
          m_auditLog(auditLog)
    {
        qCDebug(KLEOPATRA_LOG) << endl
                               << "inputError :" << m_inputErrorString << endl
                               << "outputError:" << m_outputErrorString;
        assert(!m_sresult.isNull() || !m_eresult.isNull());
    }

    QString overview() const Q_DECL_OVERRIDE;
    QString details() const Q_DECL_OVERRIDE;
    int errorCode() const Q_DECL_OVERRIDE;
    QString errorString() const Q_DECL_OVERRIDE;
    VisualCode code() const Q_DECL_OVERRIDE;
    AuditLog auditLog() const Q_DECL_OVERRIDE;

private:
    const SigningResult m_sresult;
    const EncryptionResult m_eresult;
    const QString m_inputLabel;
    const QString m_inputErrorString;
    const QString m_outputLabel;
    const QString m_outputErrorString;
    const bool m_outputCreated;
    const AuditLog m_auditLog;
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

    if (err) {
        return QString::fromLocal8Bit(err.asString()).toHtmlEscaped();
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

    if (err) {
        return QString::fromLocal8Bit(err.asString()).toHtmlEscaped();
    }
    return i18n(" Encryption succeeded.");
}

}

QString ErrorResult::overview() const
{
    assert(m_error || m_error.isCanceled());
    assert(m_sign || m_encrypt);
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

class SignEncryptFilesTask::Private
{
    friend class ::Kleo::Crypto::SignEncryptFilesTask;
    SignEncryptFilesTask *const q;
public:
    explicit Private(SignEncryptFilesTask *qq);

private:
    std::unique_ptr<QGpgME::SignJob> createSignJob(GpgME::Protocol proto);
    std::unique_ptr<QGpgME::SignEncryptJob> createSignEncryptJob(GpgME::Protocol proto);
    std::unique_ptr<QGpgME::EncryptJob> createEncryptJob(GpgME::Protocol proto);
    std::shared_ptr<const Task::Result> makeErrorResult(const Error &err, const QString &errStr, const AuditLog &auditLog);

private:
    void slotResult(const SigningResult &);
    void slotResult(const SigningResult &, const EncryptionResult &);
    void slotResult(const EncryptionResult &);

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

    QPointer<QGpgME::Job> job;
    std::shared_ptr<OverwritePolicy> m_overwritePolicy;
};

SignEncryptFilesTask::Private::Private(SignEncryptFilesTask *qq)
    : q(qq),
      input(),
      output(),
      inputFileNames(),
      outputFileName(),
      signers(),
      recipients(),
      sign(true),
      encrypt(true),
      detached(false),
      job(0),
      m_overwritePolicy(new OverwritePolicy(0))
{
    q->setAsciiArmor(true);
}

std::shared_ptr<const Task::Result> SignEncryptFilesTask::Private::makeErrorResult(const Error &err, const QString &errStr, const AuditLog &auditLog)
{
    return std::shared_ptr<const ErrorResult>(new ErrorResult(sign, encrypt, err, errStr, input->label(), output->label(), auditLog));
}

SignEncryptFilesTask::SignEncryptFilesTask(QObject *p)
    : Task(p), d(new Private(this))
{

}

SignEncryptFilesTask::~SignEncryptFilesTask() {}

void SignEncryptFilesTask::setInputFileName(const QString &fileName)
{
    kleo_assert(!d->job);
    kleo_assert(!fileName.isEmpty());
    d->inputFileNames = QStringList(fileName);
}

void SignEncryptFilesTask::setInputFileNames(const QStringList &fileNames)
{
    kleo_assert(!d->job);
    kleo_assert(!fileNames.empty());
    d->inputFileNames = fileNames;
}

void SignEncryptFilesTask::setInput(const std::shared_ptr<Input> &input)
{
    kleo_assert(!d->job);
    kleo_assert(input);
    d->input = input;
}

void SignEncryptFilesTask::setOutputFileName(const QString &fileName)
{
    kleo_assert(!d->job);
    kleo_assert(!fileName.isEmpty());
    d->outputFileName = fileName;
}

void SignEncryptFilesTask::setSigners(const std::vector<Key> &signers)
{
    kleo_assert(!d->job);
    d->signers = signers;
}

void SignEncryptFilesTask::setRecipients(const std::vector<Key> &recipients)
{
    kleo_assert(!d->job);
    d->recipients = recipients;
}

void SignEncryptFilesTask::setOverwritePolicy(const std::shared_ptr<OverwritePolicy> &policy)
{
    kleo_assert(!d->job);
    d->m_overwritePolicy = policy;
}

void SignEncryptFilesTask::setSign(bool sign)
{
    kleo_assert(!d->job);
    d->sign = sign;
}

void SignEncryptFilesTask::setEncrypt(bool encrypt)
{
    kleo_assert(!d->job);
    d->encrypt = encrypt;
}

void SignEncryptFilesTask::setDetachedSignature(bool detached)
{
    kleo_assert(!d->job);
    d->detached = detached;
}

void SignEncryptFilesTask::setEncryptSymmetric(bool symmetric)
{
    kleo_assert(!d->job);
    d->symmetric = symmetric;
}

Protocol SignEncryptFilesTask::protocol() const
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

QString SignEncryptFilesTask::label() const
{
    return d->input ? d->input->label() : QString();
}

QString SignEncryptFilesTask::tag() const
{
    return Formatting::displayName(protocol());
}

unsigned long long SignEncryptFilesTask::inputSize() const
{
    return d->input ? d->input->size() : 0U;
}

void SignEncryptFilesTask::doStart()
{
    kleo_assert(!d->job);
    if (d->sign) {
        kleo_assert(!d->signers.empty());
    }

    kleo_assert(d->input);
    d->output = Output::createFromFile(d->outputFileName, d->m_overwritePolicy);

    if (d->encrypt || d->symmetric) {
        Context::EncryptionFlags flags = Context::AlwaysTrust;
        if (d->symmetric) {
            flags = static_cast<Context::EncryptionFlags>(flags | Context::Symmetric);
            qDebug() << "Adding symmetric flag";
        }
        if (d->sign) {
            std::unique_ptr<QGpgME::SignEncryptJob> job = d->createSignEncryptJob(protocol());
            kleo_assert(job.get());

            job->start(d->signers, d->recipients,
                       d->input->ioDevice(), d->output->ioDevice(), flags);

            d->job = job.release();
        } else {
            std::unique_ptr<QGpgME::EncryptJob> job = d->createEncryptJob(protocol());
            kleo_assert(job.get());

            job->start(d->recipients, d->input->ioDevice(), d->output->ioDevice(), flags);

            d->job = job.release();
        }
    } else if (d->sign) {
        std::unique_ptr<QGpgME::SignJob> job = d->createSignJob(protocol());
        kleo_assert(job.get());

        job->start(d->signers,
                   d->input->ioDevice(), d->output->ioDevice(),
                   d->detached ? GpgME::Detached : GpgME::NormalSignatureMode);

        d->job = job.release();
    } else {
        kleo_assert(!"Either 'sign' or 'encrypt' or 'symmetric' must be set!");
    }
}

void SignEncryptFilesTask::cancel()
{
    if (d->job) {
        d->job->slotCancel();
    }
}

std::unique_ptr<QGpgME::SignJob> SignEncryptFilesTask::Private::createSignJob(GpgME::Protocol proto)
{
    const QGpgME::Protocol *const backend = (proto == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(backend);
    std::unique_ptr<QGpgME::SignJob> signJob(backend->signJob(q->asciiArmor(), /*textmode=*/false));
    kleo_assert(signJob.get());
    connect(signJob.get(), SIGNAL(progress(QString,int,int)),
            q, SLOT(setProgress(QString,int,int)));
    connect(signJob.get(), SIGNAL(result(GpgME::SigningResult,QByteArray)),
            q, SLOT(slotResult(GpgME::SigningResult)));
    return signJob;
}

std::unique_ptr<QGpgME::SignEncryptJob> SignEncryptFilesTask::Private::createSignEncryptJob(GpgME::Protocol proto)
{
    const QGpgME::Protocol *const backend = (proto == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(backend);
    std::unique_ptr<QGpgME::SignEncryptJob> signEncryptJob(backend->signEncryptJob(q->asciiArmor(), /*textmode=*/false));
    kleo_assert(signEncryptJob.get());
    connect(signEncryptJob.get(), SIGNAL(progress(QString,int,int)),
            q, SLOT(setProgress(QString,int,int)));
    connect(signEncryptJob.get(), SIGNAL(result(GpgME::SigningResult,GpgME::EncryptionResult,QByteArray)),
            q, SLOT(slotResult(GpgME::SigningResult,GpgME::EncryptionResult)));
    return signEncryptJob;
}

std::unique_ptr<QGpgME::EncryptJob> SignEncryptFilesTask::Private::createEncryptJob(GpgME::Protocol proto)
{
    const QGpgME::Protocol *const backend = (proto == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(backend);
    std::unique_ptr<QGpgME::EncryptJob> encryptJob(backend->encryptJob(q->asciiArmor(), /*textmode=*/false));
    kleo_assert(encryptJob.get());
    connect(encryptJob.get(), SIGNAL(progress(QString,int,int)),
            q, SLOT(setProgress(QString,int,int)));
    connect(encryptJob.get(), SIGNAL(result(GpgME::EncryptionResult,QByteArray)),
            q, SLOT(slotResult(GpgME::EncryptionResult)));
    return encryptJob;
}

void SignEncryptFilesTask::Private::slotResult(const SigningResult &result)
{
    const QGpgME::Job *const job = qobject_cast<const QGpgME::Job *>(q->sender());
    const AuditLog auditLog = AuditLog::fromJob(job);
    bool outputCreated = false;
    if (result.error().code()) {
        output->cancel();
    } else {
        try {
            kleo_assert(!result.isNull());
            output->finalize();
            outputCreated = true;
            input->finalize();
        } catch (const GpgME::Exception &e) {
            q->emitResult(makeErrorResult(e.error(), QString::fromLocal8Bit(e.what()), auditLog));
            return;
        }
    }

    q->emitResult(std::shared_ptr<Result>(new SignEncryptFilesResult(result, input, output, outputCreated, auditLog)));
}

void SignEncryptFilesTask::Private::slotResult(const SigningResult &sresult, const EncryptionResult &eresult)
{
    const QGpgME::Job *const job = qobject_cast<const QGpgME::Job *>(q->sender());
    const AuditLog auditLog = AuditLog::fromJob(job);
    bool outputCreated = false;
    if (sresult.error().code() || eresult.error().code()) {
        output->cancel();
    } else {
        try {
            kleo_assert(!sresult.isNull() || !eresult.isNull());
            output->finalize();
            outputCreated = true;
            input->finalize();
        } catch (const GpgME::Exception &e) {
            q->emitResult(makeErrorResult(e.error(), QString::fromLocal8Bit(e.what()), auditLog));
            return;
        }
    }

    q->emitResult(std::shared_ptr<Result>(new SignEncryptFilesResult(sresult, eresult, input, output, outputCreated, auditLog)));
}

void SignEncryptFilesTask::Private::slotResult(const EncryptionResult &result)
{
    const QGpgME::Job *const job = qobject_cast<const QGpgME::Job *>(q->sender());
    const AuditLog auditLog = AuditLog::fromJob(job);
    bool outputCreated = false;
    if (result.error().code()) {
        output->cancel();
    } else {
        try {
            kleo_assert(!result.isNull());
            output->finalize();
            outputCreated = true;
            input->finalize();
        } catch (const GpgME::Exception &e) {
            q->emitResult(makeErrorResult(e.error(), QString::fromLocal8Bit(e.what()), auditLog));
            return;
        }
    }
    q->emitResult(std::shared_ptr<Result>(new SignEncryptFilesResult(result, input, output, outputCreated, auditLog)));
}

QString SignEncryptFilesResult::overview() const
{
    const QString files = formatInputOutputLabel(m_inputLabel, m_outputLabel, !m_outputCreated);
    return files + QLatin1String(": ") + makeOverview(makeResultOverview(m_sresult, m_eresult));
}

QString SignEncryptFilesResult::details() const
{
    return errorString();
}

int SignEncryptFilesResult::errorCode() const
{
    if (m_sresult.error().code()) {
        return m_sresult.error().encodedError();
    }
    if (m_eresult.error().code()) {
        return m_eresult.error().encodedError();
    }
    return 0;
}

QString SignEncryptFilesResult::errorString() const
{
    const bool sign = !m_sresult.isNull();
    const bool encrypt = !m_eresult.isNull();

    kleo_assert(sign || encrypt);

    if (sign && encrypt) {
        return
            m_sresult.error().code() ? makeResultDetails(m_sresult, m_inputErrorString, m_outputErrorString) :
            m_eresult.error().code() ? makeResultDetails(m_eresult, m_inputErrorString, m_outputErrorString) :
            QString();
    }

    return
        sign   ? makeResultDetails(m_sresult, m_inputErrorString, m_outputErrorString) :
        /*else*/ makeResultDetails(m_eresult, m_inputErrorString, m_outputErrorString);
}

Task::Result::VisualCode SignEncryptFilesResult::code() const
{
    if (m_sresult.error().isCanceled() || m_eresult.error().isCanceled()) {
        return Warning;
    }
    return (m_sresult.error().code() || m_eresult.error().code()) ? NeutralError : NeutralSuccess;
}

AuditLog SignEncryptFilesResult::auditLog() const
{
    return m_auditLog;
}

#include "moc_signencryptfilestask.cpp"
