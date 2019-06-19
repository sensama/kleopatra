/* -*- mode: c++; c-basic-offset:4 -*-
    decryptverifytask.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB

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

#include "decryptverifytask.h"

#include <QGpgME/Protocol>
#include <QGpgME/VerifyOpaqueJob>
#include <QGpgME/VerifyDetachedJob>
#include <QGpgME/DecryptJob>
#include <QGpgME/DecryptVerifyJob>

#include <Libkleo/Dn>
#include <Libkleo/Exception>
#include <Libkleo/Stl_Util>
#include <Libkleo/KeyCache>
#include <Libkleo/Predicates>
#include <Libkleo/Formatting>
#include <Libkleo/Classify>

#include <utils/detail_p.h>
#include <utils/input.h>
#include <utils/output.h>
#include <utils/kleo_assert.h>
#include <utils/auditlog.h>
#include <utils/gnupg-helper.h>

#include <kmime/kmime_header_parsing.h>

#include <gpgme++/error.h>
#include <gpgme++/key.h>
#include <gpgme++/verificationresult.h>
#include <gpgme++/decryptionresult.h>
#include <gpgme++/gpgmepp_version.h>
#include <gpgme++/context.h>

#include <gpg-error.h>

#include "kleopatra_debug.h"
#include <KLocalizedString>
#include <QLocale>

#include <QByteArray>
#include <QDateTime>
#include <QStringList>
#include <QTextDocument> // Qt::escape

#include <algorithm>
#include <sstream>

#if GPGMEPP_VERSION > 0x10B01 // > 1.11.1
# define GPGME_HAS_LEGACY_NOMDC
#endif

using namespace Kleo::Crypto;
using namespace Kleo;
using namespace GpgME;
using namespace KMime::Types;

namespace
{

static Error make_error(const gpg_err_code_t code)
{
    return Error(gpg_error(code));
}

static AuditLog auditLogFromSender(QObject *sender)
{
    return AuditLog::fromJob(qobject_cast<const QGpgME::Job *>(sender));
}

static bool addrspec_equal(const AddrSpec &lhs, const AddrSpec &rhs, Qt::CaseSensitivity cs)
{
    return lhs.localPart.compare(rhs.localPart, cs) == 0 && lhs.domain.compare(rhs.domain, Qt::CaseInsensitive) == 0;
}

static bool mailbox_equal(const Mailbox &lhs, const Mailbox &rhs, Qt::CaseSensitivity cs)
{
    return addrspec_equal(lhs.addrSpec(), rhs.addrSpec(), cs);
}

static std::string stripAngleBrackets(const std::string &str)
{
    if (str.empty()) {
        return str;
    }
    if (str[0] == '<' && str[str.size() - 1] == '>') {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

static std::string email(const UserID &uid)
{

    if (uid.parent().protocol() == OpenPGP) {
        if (const char *const email = uid.email()) {
            return stripAngleBrackets(email);
        } else {
            return std::string();
        }
    }

    Q_ASSERT(uid.parent().protocol() == CMS);

    if (const char *const id = uid.id())
        if (*id == '<') {
            return stripAngleBrackets(id);
        } else {
            return DN(id)[QStringLiteral("EMAIL")].trimmed().toUtf8().constData();
        }
    else {
        return std::string();
    }
}

static Mailbox mailbox(const UserID &uid)
{
    const std::string e = email(uid);
    Mailbox mbox;
    if (!e.empty()) {
        mbox.setAddress(e.c_str());
    }
    return mbox;
}

static std::vector<Mailbox> extractMailboxes(const Key &key)
{
    std::vector<Mailbox> res;
    Q_FOREACH (const UserID &id, key.userIDs()) {
        const Mailbox mbox = mailbox(id);
        if (!mbox.addrSpec().isEmpty()) {
            res.push_back(mbox);
        }
    }
    return res;
}

static std::vector<Mailbox> extractMailboxes(const std::vector<Key> &signers)
{
    std::vector<Mailbox> res;
    for (const Key &i : signers) {
        const std::vector<Mailbox> bxs = extractMailboxes(i);
        res.insert(res.end(), bxs.begin(), bxs.end());
    }
    return res;
}

static bool keyContainsMailbox(const Key &key, const Mailbox &mbox)
{
    const std::vector<Mailbox> mbxs = extractMailboxes(key);
    return std::find_if(mbxs.cbegin(), mbxs.cend(), 
                        [mbox](const Mailbox &m) {
                            return mailbox_equal(mbox, m, Qt::CaseInsensitive);
                        }) != mbxs.cend();
}

static bool keysContainMailbox(const std::vector<Key> &keys, const Mailbox &mbox)
{
    return std::find_if(keys.cbegin(), keys.cend(), 
                        [mbox](const Key &key) {
                            return keyContainsMailbox(key, mbox);
                        }) != keys.cend();
}

static bool relevantInDecryptVerifyContext(const VerificationResult &r)
{
    // for D/V operations, we ignore verification results which are not errors and contain
    // no signatures (which means that the data was just not signed)

    return (r.error() && r.error().code() != GPG_ERR_DECRYPT_FAILED) || r.numSignatures() > 0;
}

static QString signatureSummaryToString(int summary)
{
    if (summary & Signature::None) {
        return i18n("Error: Signature not verified");
    } else if (summary & Signature::Valid || summary & Signature::Green) {
        return i18n("Good signature");
    } else if (summary & Signature::KeyRevoked) {
        return i18n("Signing certificate was revoked");
    } else if (summary & Signature::KeyExpired) {
        return i18n("Signing certificate is expired");
    } else if (summary & Signature::KeyMissing) {
        return i18n("Certificate is not available");
    } else if (summary & Signature::SigExpired) {
        return i18n("Signature expired");
    } else if (summary & Signature::CrlMissing) {
        return i18n("CRL missing");
    } else if (summary & Signature::CrlTooOld) {
        return i18n("CRL too old");
    } else if (summary & Signature::BadPolicy) {
        return i18n("Bad policy");
    } else if (summary & Signature::SysError) {
        return i18n("System error");    //### retrieve system error details?
    } else if (summary & Signature::Red) {
        return i18n("Bad signature");
    }return QString();
}

static QString formatValidSignatureWithTrustLevel(const UserID &id)
{
    if (id.isNull()) {
        return QString();
    }
    switch (id.validity()) {
    case UserID::Marginal:
        return i18n("The signature is valid but the trust in the certificate's validity is only marginal.");
    case UserID::Full:
        return i18n("The signature is valid and the certificate's validity is fully trusted.");
    case UserID::Ultimate:
        return i18n("The signature is valid and the certificate's validity is ultimately trusted.");
    case UserID::Never:
        return i18n("The signature is valid but the certificate's validity is <em>not trusted</em>.");
    case UserID::Unknown:
        return i18n("The signature is valid but the certificate's validity is unknown.");
    case UserID::Undefined:
    default:
        return i18n("The signature is valid but the certificate's validity is undefined.");
    }
}

static QString renderKeyLink(const QString &fpr, const QString &text)
{
    return QStringLiteral("<a href=\"key:%1\">%2</a>").arg(fpr, text);
}

static QString renderKey(const Key &key)
{
    if (key.isNull()) {
        return i18n("Unknown certificate");
    }

    if (key.primaryFingerprint() && strlen(key.primaryFingerprint()) > 16 && key.numUserIDs()) {
        const QString text = QStringLiteral("%1 (%2)").arg(Formatting::prettyNameAndEMail(key).toHtmlEscaped()).arg(
            Formatting::prettyID(QString::fromLocal8Bit(key.primaryFingerprint()).right(16).toLatin1().constData()));
        return renderKeyLink(QLatin1String(key.primaryFingerprint()), text);
    }

    return renderKeyLink(QLatin1String(key.primaryFingerprint()), Formatting::prettyID(key.primaryFingerprint()));
}

static QString renderKeyEMailOnlyNameAsFallback(const Key &key)
{
    if (key.isNull()) {
        return i18n("Unknown certificate");
    }
    const QString email = Formatting::prettyEMail(key);
    const QString user = !email.isEmpty() ? email : Formatting::prettyName(key);
    return renderKeyLink(QLatin1String(key.primaryFingerprint()), user);
}

static QString formatDate(const QDateTime &dt)
{
    return QLocale().toString(dt);
}
static QString formatSigningInformation(const Signature &sig)
{
    if (sig.isNull()) {
        return QString();
    }
    const QDateTime dt = sig.creationTime() != 0 ? QDateTime::fromSecsSinceEpoch(sig.creationTime()) : QDateTime();
    QString text;
    Key key = sig.key();
    if (dt.isValid()) {
        text = i18nc("1 is a date", "Signature created on %1", formatDate(dt)) + QStringLiteral("<br>");
    }
    if (key.isNull()) {
        return text += i18n("With unavailable certificate:") + QStringLiteral("<br>ID: 0x%1").arg(QString::fromLatin1(sig.fingerprint()).toUpper());
    }
    text += i18n("With certificate:") + QStringLiteral("<br>") + renderKey(key);

    if (Kleo::gpgComplianceP("de-vs")) {
        text +=
            (QStringLiteral("<br/>")
             + (IS_DE_VS(sig)
                ? i18nc("VS-NfD-conforming is a German standard for restricted documents for which special restrictions about algorithms apply.  The string states that a signature is compliant with that.",
                        "The signature is VS-NfD-compliant.")
                : i18nc("VS-NfD-conforming is a German standard for restricted documents for which special restrictions about algorithms apply.  The string states that a signature is not compliant with that.",
                        "The signature <b>is not</b> VS-NfD-compliant.")));
    }

    return text;
}

static QString strikeOut(const QString &str, bool strike)
{
    return QString(strike ? QStringLiteral("<s>%1</s>") : QStringLiteral("%1")).arg(str.toHtmlEscaped());
}

static QString formatInputOutputLabel(const QString &input, const QString &output, bool inputDeleted, bool outputDeleted)
{
    if (output.isEmpty()) {
        return strikeOut(input, inputDeleted);
    }
    return i18nc("Input file --> Output file (rarr is arrow", "%1 &rarr; %2",
                 strikeOut(input, inputDeleted),
                 strikeOut(output, outputDeleted));
}

static bool IsErrorOrCanceled(const GpgME::Error &err)
{
    return err || err.isCanceled();
}

static bool IsErrorOrCanceled(const Result &res)
{
    return IsErrorOrCanceled(res.error());
}

static bool IsBad(const Signature &sig)
{
    return sig.summary() & Signature::Red;
}

static bool IsGoodOrValid(const Signature &sig)
{
    return (sig.summary() & Signature::Valid) || (sig.summary() & Signature::Green);
}

static UserID findUserIDByMailbox(const Key &key, const Mailbox &mbox)
{
    Q_FOREACH (const UserID &id, key.userIDs())
        if (mailbox_equal(mailbox(id), mbox, Qt::CaseInsensitive)) {
            return id;
        }
    return UserID();
}

static void updateKeys(const VerificationResult &result) {
    // This little hack works around the problem that GnuPG / GpgME does not
    // provide Key information in a verification result. The Key object is
    // a dummy just holding the KeyID. This hack ensures that all available
    // keys are fetched from the backend and are populated
    for (const auto &sig: result.signatures()) {
        // Update key information
        sig.key(true, true);
    }
}

}

class DecryptVerifyResult::SenderInfo
{
public:
    explicit SenderInfo(const Mailbox &infSender, const std::vector<Key> &signers_) : informativeSender(infSender), signers(signers_) {}
    const Mailbox informativeSender;
    const std::vector<Key> signers;
    bool hasInformativeSender() const
    {
        return !informativeSender.addrSpec().isEmpty();
    }
    bool conflicts() const
    {
        return hasInformativeSender() && hasKeys() && !keysContainMailbox(signers, informativeSender);
    }
    bool hasKeys() const
    {
        return std::any_of(signers.cbegin(), signers.cend(), [](const Key &key) { return !key.isNull(); });
    }
    std::vector<Mailbox> signerMailboxes() const
    {
        return extractMailboxes(signers);
    }
};

namespace
{

static Task::Result::VisualCode codeForVerificationResult(const VerificationResult &res)
{
    if (res.isNull()) {
        return Task::Result::NeutralSuccess;
    }

    const std::vector<Signature> sigs = res.signatures();
    if (sigs.empty()) {
        return Task::Result::Warning;
    }

    if (std::find_if(sigs.begin(), sigs.end(), IsBad) != sigs.end()) {
        return Task::Result::Danger;
    }

    if ((size_t)std::count_if(sigs.begin(), sigs.end(), IsGoodOrValid) == sigs.size()) {
        return Task::Result::AllGood;
    }

    return Task::Result::Warning;
}

static QString formatVerificationResultOverview(const VerificationResult &res, const DecryptVerifyResult::SenderInfo &info)
{
    if (res.isNull()) {
        return QString();
    }

    const Error err = res.error();

    if (err.isCanceled()) {
        return i18n("<b>Verification canceled.</b>");
    } else if (err) {
        return i18n("<b>Verification failed: %1.</b>", QString::fromLocal8Bit(err.asString()).toHtmlEscaped());
    }

    const std::vector<Signature> sigs = res.signatures();

    if (sigs.empty()) {
        return i18n("<b>No signatures found.</b>");
    }

    const uint bad = std::count_if(sigs.cbegin(), sigs.cend(), IsBad);
    if (bad > 0) {
        return i18np("<b>Invalid signature.</b>", "<b>%1 invalid signatures.</b>", bad);
    }
    const uint warn = std::count_if(sigs.cbegin(), sigs.cend(), [](const Signature &sig) { return !IsGoodOrValid(sig); });
    if (warn == sigs.size()) {
        return i18np("<b>The data could not be verified.</b>", "<b>%1 signatures could not be verified.</b>", warn);
    }

    //Good signature:
    QString text;
    if (sigs.size() == 1) {
        text = i18n("<b>Valid signature by %1</b>", renderKeyEMailOnlyNameAsFallback(sigs[0].key()));
        if (info.conflicts())
            text += i18n("<br/><b>Warning:</b> The sender's mail address is not stored in the %1 used for signing.",
                         renderKeyLink(QLatin1String(sigs[0].key().primaryFingerprint()), i18n("certificate")));
    } else {
        text = i18np("<b>Valid signature.</b>", "<b>%1 valid signatures.</b>", sigs.size());
        if (info.conflicts()) {
            text += i18n("<br/><b>Warning:</b> The sender's mail address is not stored in the certificates used for signing.");
        }
    }

    return text;
}

static QString formatDecryptionResultOverview(const DecryptionResult &result, const QString &errorString = QString())
{
    const Error err = result.error();

    if (err.isCanceled()) {
        return i18n("<b>Decryption canceled.</b>");
    }
#ifdef GPGME_HAS_LEGACY_NOMDC
    else if (result.isLegacyCipherNoMDC()) {
        return i18n("<b>Decryption failed: %1.</b>", i18n("No integrity protection (MDC)."));
    }
#endif
    else if (!errorString.isEmpty()) {
        return i18n("<b>Decryption failed: %1.</b>", errorString.toHtmlEscaped());
    } else if (err) {
        return i18n("<b>Decryption failed: %1.</b>", QString::fromLocal8Bit(err.asString()).toHtmlEscaped());
    }
    return i18n("<b>Decryption succeeded.</b>");
}

static QString formatSignature(const Signature &sig, const DecryptVerifyResult::SenderInfo &info)
{
    if (sig.isNull()) {
        return QString();
    }

    const QString text = formatSigningInformation(sig) + QLatin1String("<br/>");
    const Key key = sig.key();

    // Green
    if (sig.summary() & Signature::Valid) {
        const UserID id = findUserIDByMailbox(key, info.informativeSender);
        return text + formatValidSignatureWithTrustLevel(!id.isNull() ? id : key.userID(0));
    }

    // Red
    if ((sig.summary() & Signature::Red)) {
        const QString ret = text + i18n("The signature is invalid: %1", signatureSummaryToString(sig.summary()));
        if (sig.summary() & Signature::SysError) {
            return ret + QStringLiteral(" (%1)").arg(QString::fromLocal8Bit(sig.status().asString()));
        }
        return ret;
    }

    // Key missing
    if ((sig.summary() & Signature::KeyMissing)) {
        return text + i18n("You can search the certificate on a keyserver or import it from a file.");
    }

    // Yellow
    if ((sig.validity() & Signature::Validity::Undefined) ||
        (sig.validity() & Signature::Validity::Unknown) ||
        (sig.summary() == Signature::Summary::None)) {
        return text + (key.protocol() == OpenPGP ? i18n("The used key is not certified by you or any trusted person.") :
               i18n("The used certificate is not certified by a trustworthy Certificate Authority or the Certificate Authority is unknown."));
    }

    // Catch all fall through
    const QString ret = text + i18n("The signature is invalid: %1", signatureSummaryToString(sig.summary()));
    if (sig.summary() & Signature::SysError) {
        return ret + QStringLiteral(" (%1)").arg(QString::fromLocal8Bit(sig.status().asString()));
    }
    return ret;
}

static QStringList format(const std::vector<Mailbox> &mbxs)
{
    QStringList res;
    std::transform(mbxs.cbegin(), mbxs.cend(), std::back_inserter(res),
                   [](const Mailbox &mbox) {
                       return mbox.prettyAddress();
                   });
    return res;
}

static QString formatVerificationResultDetails(const VerificationResult &res, const DecryptVerifyResult::SenderInfo &info, const QString &errorString)
{
    if ((res.error().code() == GPG_ERR_EIO || res.error().code() == GPG_ERR_NO_DATA) && !errorString.isEmpty()) {
        return i18n("Input error: %1", errorString);
    }

    const std::vector<Signature> sigs = res.signatures();
    QString details;
    for (const Signature &sig : sigs) {
        details += formatSignature(sig, info) + QLatin1Char('\n');
    }
    details = details.trimmed();
    details.replace(QLatin1Char('\n'), QStringLiteral("<br/><br/>"));
    if (info.conflicts()) {
        details += i18n("<p>The sender's address %1 is not stored in the certificate. Stored: %2</p>", info.informativeSender.prettyAddress(), format(info.signerMailboxes()).join(i18nc("separator for a list of e-mail addresses", ", ")));
    }
    return details;
}

static QString formatDecryptionResultDetails(const DecryptionResult &res, const std::vector<Key> &recipients,
                                             const QString &errorString, bool isSigned, const QPointer<Task> &task)
{
    QString details;

    if ((res.error().code() == GPG_ERR_EIO || res.error().code() == GPG_ERR_NO_DATA) && !errorString.isEmpty()) {
        return i18n("Input error: %1", errorString);
    }

    if (Kleo::gpgComplianceP("de-vs")) {
        details += ((IS_DE_VS(res)
                     ? i18nc("VS-NfD-conforming is a German standard for restricted documents for which special restrictions about algorithms apply.  The string states that the decryption is compliant with that.",
                             "The decryption is VS-NfD-compliant.")
                     : i18nc("VS-NfD-conforming is a German standard for restricted documents for which special restrictions about algorithms apply.  The string states that the decryption is compliant with that.",
                             "The decryption <b>is not</b> VS-NfD-compliant."))
                    + QStringLiteral("<br/>"));
    }

    if (res.fileName()) {
        const auto decVerifyTask = qobject_cast<AbstractDecryptVerifyTask*> (task.data());
        if (decVerifyTask) {
            const auto embedFileName = QString::fromUtf8(res.fileName()).toHtmlEscaped();

            if (embedFileName != decVerifyTask->outputLabel()) {
                details += i18n("Embedded file name: '%1'", embedFileName);
                details += QStringLiteral("<br/>");
            }
        }
    }

    if (res.isNull() || !res.error() || res.error().isCanceled()) {
        if (!isSigned) {
            return details + i18n("<b>Note:</b> You cannot be sure who encrypted this message as it is not signed.");
        }
        return details;
    }

    if (recipients.empty() && res.numRecipients() > 0) {
        return details + QLatin1String("<i>") + i18np("One unknown recipient.", "%1 unknown recipients.", res.numRecipients()) + QLatin1String("</i>");
    }

#ifdef GPGME_HAS_LEGACY_NOMDC
    if (res.isLegacyCipherNoMDC()) {
        details += i18nc("Integrity protection was missing because an old cipher was used.",
                         "<b>Hint:</b> If this file was encrypted before the year 2003 it is "
                         "likely that the file is legitimate.  This is because back "
                         "then integrity protection was not widely used.") + QStringLiteral("<br/><br/>") +
                   i18nc("The user is offered to force decrypt a non integrity protected message. With the strong advice to re-encrypt it.",
                         "If you are confident that the file was not manipulated you should re-encrypt it after you have forced the decryption.") +
                   QStringLiteral("<br/><br/>");
    }
#endif

    if (!recipients.empty()) {
        details += i18np("Recipient:", "Recipients:", res.numRecipients());
        if (res.numRecipients() == 1) {
            return details + QLatin1Char(' ') + renderKey(recipients.front());
        }

        details += QLatin1String("<ul>");
        for (const Key &key : recipients) {
            details += QLatin1String("<li>") + renderKey(key) + QLatin1String("</li>");
        }
        if (recipients.size() < res.numRecipients())
            details += QLatin1String("<li><i>") + i18np("One unknown recipient", "%1 unknown recipients",
                       res.numRecipients() - recipients.size()) + QLatin1String("</i></li>");

        details += QLatin1String("</ul>");
    }

    return details;
}

static QString formatDecryptVerifyResultOverview(const DecryptionResult &dr, const VerificationResult &vr, const  DecryptVerifyResult::SenderInfo &info)
{
    if (IsErrorOrCanceled(dr) || !relevantInDecryptVerifyContext(vr)) {
        return formatDecryptionResultOverview(dr);
    }
    return formatVerificationResultOverview(vr, info);
}

static QString formatDecryptVerifyResultDetails(const DecryptionResult &dr,
        const VerificationResult &vr,
        const std::vector<Key> &recipients,
        const DecryptVerifyResult::SenderInfo &info,
        const QString &errorString,
        const QPointer<Task> &task)
{
    const QString drDetails = formatDecryptionResultDetails(dr, recipients, errorString, relevantInDecryptVerifyContext(vr), task);
    if (IsErrorOrCanceled(dr) || !relevantInDecryptVerifyContext(vr)) {
        return drDetails;
    }
    return drDetails + (drDetails.isEmpty() ? QString() : QStringLiteral("<br/>")) + formatVerificationResultDetails(vr, info, errorString);
}

} // anon namespace

class DecryptVerifyResult::Private
{
    DecryptVerifyResult *const q;
public:
    Private(DecryptVerifyOperation type,
            const VerificationResult &vr,
            const DecryptionResult &dr,
            const QByteArray &stuff,
            int errCode,
            const QString &errString,
            const QString &input,
            const QString &output,
            const AuditLog &auditLog,
            Task *parentTask,
            const Mailbox &informativeSender,
            DecryptVerifyResult *qq) :
        q(qq),
        m_type(type),
        m_verificationResult(vr),
        m_decryptionResult(dr),
        m_stuff(stuff),
        m_error(errCode),
        m_errorString(errString),
        m_inputLabel(input),
        m_outputLabel(output),
        m_auditLog(auditLog),
        m_parentTask(QPointer<Task>(parentTask)),
        m_informativeSender(informativeSender)
    {
    }

    QString label() const
    {
        return formatInputOutputLabel(m_inputLabel, m_outputLabel, false, q->hasError());
    }

    DecryptVerifyResult::SenderInfo makeSenderInfo() const;

    bool isDecryptOnly() const
    {
        return m_type == Decrypt;
    }
    bool isVerifyOnly() const
    {
        return m_type == Verify;
    }
    bool isDecryptVerify() const
    {
        return m_type == DecryptVerify;
    }
    DecryptVerifyOperation m_type;
    VerificationResult m_verificationResult;
    DecryptionResult m_decryptionResult;
    QByteArray m_stuff;
    int m_error;
    QString m_errorString;
    QString m_inputLabel;
    QString m_outputLabel;
    const AuditLog m_auditLog;
    QPointer <Task> m_parentTask;
    const Mailbox m_informativeSender;
};

DecryptVerifyResult::SenderInfo DecryptVerifyResult::Private::makeSenderInfo() const
{
    return SenderInfo(m_informativeSender, KeyCache::instance()->findSigners(m_verificationResult));
}

std::shared_ptr<DecryptVerifyResult> AbstractDecryptVerifyTask::fromDecryptResult(const DecryptionResult &dr, const QByteArray &plaintext, const AuditLog &auditLog)
{
    return std::shared_ptr<DecryptVerifyResult>(new DecryptVerifyResult(
            Decrypt,
            VerificationResult(),
            dr,
            plaintext,
            0,
            QString(),
            inputLabel(),
            outputLabel(),
            auditLog,
            this,
            informativeSender()));
}

std::shared_ptr<DecryptVerifyResult> AbstractDecryptVerifyTask::fromDecryptResult(const GpgME::Error &err, const QString &what, const AuditLog &auditLog)
{
    return std::shared_ptr<DecryptVerifyResult>(new DecryptVerifyResult(
            Decrypt,
            VerificationResult(),
            DecryptionResult(err),
            QByteArray(),
            err.code(),
            what,
            inputLabel(),
            outputLabel(),
            auditLog,
            this,
            informativeSender()));
}

std::shared_ptr<DecryptVerifyResult> AbstractDecryptVerifyTask::fromDecryptVerifyResult(const DecryptionResult &dr, const VerificationResult &vr, const QByteArray &plaintext, const AuditLog &auditLog)
{
    int err = dr.error() ? dr.error().code() : vr.error().code();
    return std::shared_ptr<DecryptVerifyResult>(new DecryptVerifyResult(
            DecryptVerify,
            vr,
            dr,
            plaintext,
            err,
            QString(),
            inputLabel(),
            outputLabel(),
            auditLog,
            this,
            informativeSender()));
}

std::shared_ptr<DecryptVerifyResult> AbstractDecryptVerifyTask::fromDecryptVerifyResult(const GpgME::Error &err, const QString &details, const AuditLog &auditLog)
{
    return std::shared_ptr<DecryptVerifyResult>(new DecryptVerifyResult(
            DecryptVerify,
            VerificationResult(),
            DecryptionResult(err),
            QByteArray(),
            err.code(),
            details,
            inputLabel(),
            outputLabel(),
            auditLog,
            this,
            informativeSender()));
}

std::shared_ptr<DecryptVerifyResult> AbstractDecryptVerifyTask::fromVerifyOpaqueResult(const VerificationResult &vr, const QByteArray &plaintext, const AuditLog &auditLog)
{
    return std::shared_ptr<DecryptVerifyResult>(new DecryptVerifyResult(
            Verify,
            vr,
            DecryptionResult(),
            plaintext,
            0,
            QString(),
            inputLabel(),
            outputLabel(),
            auditLog,
            this,
            informativeSender()));
}
std::shared_ptr<DecryptVerifyResult> AbstractDecryptVerifyTask::fromVerifyOpaqueResult(const GpgME::Error &err, const QString &details, const AuditLog &auditLog)
{
    return std::shared_ptr<DecryptVerifyResult>(new DecryptVerifyResult(
            Verify,
            VerificationResult(err),
            DecryptionResult(),
            QByteArray(),
            err.code(),
            details,
            inputLabel(),
            outputLabel(),
            auditLog,
            this,
            informativeSender()));
}

std::shared_ptr<DecryptVerifyResult> AbstractDecryptVerifyTask::fromVerifyDetachedResult(const VerificationResult &vr, const AuditLog &auditLog)
{
    return std::shared_ptr<DecryptVerifyResult>(new DecryptVerifyResult(
            Verify,
            vr,
            DecryptionResult(),
            QByteArray(),
            0,
            QString(),
            inputLabel(),
            outputLabel(),
            auditLog,
            this,
            informativeSender()));
}
std::shared_ptr<DecryptVerifyResult> AbstractDecryptVerifyTask::fromVerifyDetachedResult(const GpgME::Error &err, const QString &details, const AuditLog &auditLog)
{
    return std::shared_ptr<DecryptVerifyResult>(new DecryptVerifyResult(
            Verify,
            VerificationResult(err),
            DecryptionResult(),
            QByteArray(),
            err.code(),
            details,
            inputLabel(),
            outputLabel(),
            auditLog,
            this,
            informativeSender()));
}

DecryptVerifyResult::DecryptVerifyResult(DecryptVerifyOperation type,
        const VerificationResult &vr,
        const DecryptionResult &dr,
        const QByteArray &stuff,
        int errCode,
        const QString &errString,
        const QString &inputLabel,
        const QString &outputLabel,
        const AuditLog &auditLog,
        Task *parentTask,
        const Mailbox &informativeSender)
    : Task::Result(), d(new Private(type, vr, dr, stuff, errCode, errString, inputLabel, outputLabel, auditLog, parentTask, informativeSender, this))
{
}

QString DecryptVerifyResult::overview() const
{
    QString ov;
    if (d->isDecryptOnly()) {
        ov += formatDecryptionResultOverview(d->m_decryptionResult);
    } else if (d->isVerifyOnly()) {
        ov += formatVerificationResultOverview(d->m_verificationResult, d->makeSenderInfo());
    } else {
        ov += formatDecryptVerifyResultOverview(d->m_decryptionResult, d->m_verificationResult, d->makeSenderInfo());
    }
    if (ov.size() + d->label().size() > 120) {
        // Avoid ugly breaks
        ov = QStringLiteral("<br>") + ov;
    }
    return i18nc("label: result example: foo.sig: Verification failed. ", "%1: %2", d->label(), ov);
}

QString DecryptVerifyResult::details() const
{
    if (d->isDecryptOnly()) {
        return formatDecryptionResultDetails(d->m_decryptionResult,
                KeyCache::instance()->findRecipients(d->m_decryptionResult),
                errorString(), false, d->m_parentTask);
    }
    if (d->isVerifyOnly()) {
        return formatVerificationResultDetails(d->m_verificationResult, d->makeSenderInfo(), errorString());
    }
    return formatDecryptVerifyResultDetails(d->m_decryptionResult,
                                            d->m_verificationResult, KeyCache::instance()->findRecipients(
                                                    d->m_decryptionResult), d->makeSenderInfo(), errorString(),
                                            d->m_parentTask);
}

bool DecryptVerifyResult::hasError() const
{
    return d->m_error != 0;
}

int DecryptVerifyResult::errorCode() const
{
    return d->m_error;
}

QString DecryptVerifyResult::errorString() const
{
    return d->m_errorString;
}

AuditLog DecryptVerifyResult::auditLog() const
{
    return d->m_auditLog;
}

QPointer<Task> DecryptVerifyResult::parentTask() const
{
    return d->m_parentTask;
}

Task::Result::VisualCode DecryptVerifyResult::code() const
{
    if ((d->m_type == DecryptVerify || d->m_type == Verify) && relevantInDecryptVerifyContext(verificationResult())) {
        return codeForVerificationResult(verificationResult());
    }
    return hasError() ? NeutralError : NeutralSuccess;
}

GpgME::VerificationResult DecryptVerifyResult::verificationResult() const
{
    return d->m_verificationResult;
}

GpgME::DecryptionResult DecryptVerifyResult::decryptionResult() const
{
    return d->m_decryptionResult;
}

class AbstractDecryptVerifyTask::Private
{
public:
    Mailbox informativeSender;
};

AbstractDecryptVerifyTask::AbstractDecryptVerifyTask(QObject *parent) : Task(parent), d(new Private) {}

AbstractDecryptVerifyTask::~AbstractDecryptVerifyTask() {}

Mailbox AbstractDecryptVerifyTask::informativeSender() const
{
    return d->informativeSender;
}

void AbstractDecryptVerifyTask::setInformativeSender(const Mailbox &sender)
{
    d->informativeSender = sender;
}

class DecryptVerifyTask::Private
{
    DecryptVerifyTask *const q;
public:
    explicit Private(DecryptVerifyTask *qq) : q(qq), m_backend(nullptr), m_protocol(UnknownProtocol), m_ignoreMDCError(false)  {}

    void slotResult(const DecryptionResult &, const VerificationResult &, const QByteArray &);

    void registerJob(QGpgME::DecryptVerifyJob *job)
    {
        q->connect(job, SIGNAL(result(GpgME::DecryptionResult,GpgME::VerificationResult,QByteArray)),
                   q, SLOT(slotResult(GpgME::DecryptionResult,GpgME::VerificationResult,QByteArray)));
        q->connect(job, SIGNAL(progress(QString,int,int)),
                   q, SLOT(setProgress(QString,int,int)));
    }

    void emitResult(const std::shared_ptr<DecryptVerifyResult> &result);

    std::shared_ptr<Input> m_input;
    std::shared_ptr<Output> m_output;
    const QGpgME::Protocol *m_backend;
    Protocol m_protocol;
    bool m_ignoreMDCError;
};

void DecryptVerifyTask::Private::emitResult(const std::shared_ptr<DecryptVerifyResult> &result)
{
    q->emitResult(result);
    Q_EMIT q->decryptVerifyResult(result);
}

void DecryptVerifyTask::Private::slotResult(const DecryptionResult &dr, const VerificationResult &vr, const QByteArray &plainText)
{
    updateKeys(vr);
    {
        std::stringstream ss;
        ss << dr << '\n' << vr;
        qCDebug(KLEOPATRA_LOG) << ss.str().c_str();
    }
    const AuditLog auditLog = auditLogFromSender(q->sender());
    if (dr.error().code() || vr.error().code()) {
        m_output->cancel();
    } else {
        try {
            kleo_assert(!dr.isNull() || !vr.isNull());
            m_output->finalize();
        } catch (const GpgME::Exception &e) {
            emitResult(q->fromDecryptResult(e.error(), QString::fromLocal8Bit(e.what()), auditLog));
            return;
        } catch (const std::exception &e) {
            emitResult(q->fromDecryptResult(make_error(GPG_ERR_INTERNAL), i18n("Caught exception: %1", QString::fromLocal8Bit(e.what())), auditLog));
            return;
        } catch (...) {
            emitResult(q->fromDecryptResult(make_error(GPG_ERR_INTERNAL), i18n("Caught unknown exception"), auditLog));
            return;
        }
    }
    const int drErr = dr.error().code();
    const QString errorString = m_output->errorString();
    if (((drErr == GPG_ERR_EIO || drErr == GPG_ERR_NO_DATA) && !errorString.isEmpty()) ||
          m_output->failed()) {
        emitResult(q->fromDecryptResult(drErr ? dr.error() : Error::fromCode(GPG_ERR_EIO),
                    errorString, auditLog));
        return;
    }

    emitResult(q->fromDecryptVerifyResult(dr, vr, plainText, auditLog));
}

DecryptVerifyTask::DecryptVerifyTask(QObject *parent) : AbstractDecryptVerifyTask(parent), d(new Private(this))
{
}

DecryptVerifyTask::~DecryptVerifyTask()
{
}

void DecryptVerifyTask::setInput(const std::shared_ptr<Input> &input)
{
    d->m_input = input;
    kleo_assert(d->m_input && d->m_input->ioDevice());
}

void DecryptVerifyTask::setOutput(const std::shared_ptr<Output> &output)
{
    d->m_output = output;
    kleo_assert(d->m_output && d->m_output->ioDevice());
}

void DecryptVerifyTask::setProtocol(Protocol prot)
{
    kleo_assert(prot != UnknownProtocol);
    d->m_protocol = prot;
    d->m_backend = prot == GpgME::OpenPGP ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(d->m_backend);
}

void DecryptVerifyTask::autodetectProtocolFromInput()
{
    if (!d->m_input) {
        return;
    }
    const Protocol p = findProtocol(d->m_input->classification());
    if (p == UnknownProtocol) {
        throw Exception(gpg_error(GPG_ERR_NOTHING_FOUND), i18n("Could not determine whether this is an S/MIME or an OpenPGP signature/ciphertext - maybe it is neither ciphertext nor a signature?"), Exception::MessageOnly);
    }
    setProtocol(p);
}

QString DecryptVerifyTask::label() const
{
    return i18n("Decrypting: %1...", d->m_input->label());
}

unsigned long long DecryptVerifyTask::inputSize() const
{
    return d->m_input ? d->m_input->size() : 0;
}

QString DecryptVerifyTask::inputLabel() const
{
    return d->m_input ? d->m_input->label() : QString();
}

QString DecryptVerifyTask::outputLabel() const
{
    return d->m_output ? d->m_output->label() : QString();
}

Protocol DecryptVerifyTask::protocol() const
{
    return d->m_protocol;
}

void DecryptVerifyTask::cancel()
{

}

static void ensureIOOpen(QIODevice *input, QIODevice *output)
{
    if (input && !input->isOpen()) {
        input->open(QIODevice::ReadOnly);
    }
    if (output && !output->isOpen()) {
        output->open(QIODevice::WriteOnly);
    }
}

void DecryptVerifyTask::setIgnoreMDCError(bool value)
{
    d->m_ignoreMDCError = value;
}

void DecryptVerifyTask::doStart()
{
    kleo_assert(d->m_backend);
    try {
        QGpgME::DecryptVerifyJob *const job = d->m_backend->decryptVerifyJob();

#ifdef GPGME_HAS_LEGACY_NOMDC
        if (d->m_ignoreMDCError) {
            qCDebug(KLEOPATRA_LOG) << "Modifying job to ignore MDC errors.";
            auto ctx = QGpgME::Job::context(job);
            if (!ctx) {
                qCWarning(KLEOPATRA_LOG) << "Failed to get context for job";
            } else {
                const auto err = ctx->setFlag("ignore-mdc-error", "1");
                if (err) {
                    qCWarning(KLEOPATRA_LOG) << "Failed to set ignore mdc errors" << err.asString();
                }
            }
        }
#endif
        kleo_assert(job);
        d->registerJob(job);
        ensureIOOpen(d->m_input->ioDevice().get(), d->m_output->ioDevice().get());
        job->start(d->m_input->ioDevice(), d->m_output->ioDevice());
    } catch (const GpgME::Exception &e) {
        d->emitResult(fromDecryptVerifyResult(e.error(), QString::fromLocal8Bit(e.what()), AuditLog()));
    } catch (const std::exception &e) {
        d->emitResult(fromDecryptVerifyResult(make_error(GPG_ERR_INTERNAL), i18n("Caught exception: %1", QString::fromLocal8Bit(e.what())), AuditLog()));
    } catch (...) {
        d->emitResult(fromDecryptVerifyResult(make_error(GPG_ERR_INTERNAL), i18n("Caught unknown exception"), AuditLog()));
    }

}

class DecryptTask::Private
{
    DecryptTask *const q;
public:
    explicit Private(DecryptTask *qq) : q(qq), m_backend(nullptr), m_protocol(UnknownProtocol)  {}

    void slotResult(const DecryptionResult &, const QByteArray &);

    void registerJob(QGpgME::DecryptJob *job)
    {
        q->connect(job, SIGNAL(result(GpgME::DecryptionResult,QByteArray)),
                   q, SLOT(slotResult(GpgME::DecryptionResult,QByteArray)));
        q->connect(job, SIGNAL(progress(QString,int,int)),
                   q, SLOT(setProgress(QString,int,int)));
    }

    void emitResult(const std::shared_ptr<DecryptVerifyResult> &result);

    std::shared_ptr<Input> m_input;
    std::shared_ptr<Output> m_output;
    const QGpgME::Protocol *m_backend;
    Protocol m_protocol;
};

void DecryptTask::Private::emitResult(const std::shared_ptr<DecryptVerifyResult> &result)
{
    q->emitResult(result);
    Q_EMIT q->decryptVerifyResult(result);
}

void DecryptTask::Private::slotResult(const DecryptionResult &result, const QByteArray &plainText)
{
    {
        std::stringstream ss;
        ss << result;
        qCDebug(KLEOPATRA_LOG) << ss.str().c_str();
    }
    const AuditLog auditLog = auditLogFromSender(q->sender());
    if (result.error().code()) {
        m_output->cancel();
    } else {
        try {
            kleo_assert(!result.isNull());
            m_output->finalize();
        } catch (const GpgME::Exception &e) {
            emitResult(q->fromDecryptResult(e.error(), QString::fromLocal8Bit(e.what()), auditLog));
            return;
        } catch (const std::exception &e) {
            emitResult(q->fromDecryptResult(make_error(GPG_ERR_INTERNAL), i18n("Caught exception: %1", QString::fromLocal8Bit(e.what())), auditLog));
            return;
        } catch (...) {
            emitResult(q->fromDecryptResult(make_error(GPG_ERR_INTERNAL), i18n("Caught unknown exception"), auditLog));
            return;
        }
    }

    const int drErr = result.error().code();
    const QString errorString = m_output->errorString();
    if (((drErr == GPG_ERR_EIO || drErr == GPG_ERR_NO_DATA) && !errorString.isEmpty()) ||
          m_output->failed()) {
        emitResult(q->fromDecryptResult(result.error() ? result.error() : Error::fromCode(GPG_ERR_EIO),
                    errorString, auditLog));
        return;
    }

    emitResult(q->fromDecryptResult(result, plainText, auditLog));
}

DecryptTask::DecryptTask(QObject *parent) : AbstractDecryptVerifyTask(parent), d(new Private(this))
{
}

DecryptTask::~DecryptTask()
{
}

void DecryptTask::setInput(const std::shared_ptr<Input> &input)
{
    d->m_input = input;
    kleo_assert(d->m_input && d->m_input->ioDevice());
}

void DecryptTask::setOutput(const std::shared_ptr<Output> &output)
{
    d->m_output = output;
    kleo_assert(d->m_output && d->m_output->ioDevice());
}

void DecryptTask::setProtocol(Protocol prot)
{
    kleo_assert(prot != UnknownProtocol);
    d->m_protocol = prot;
    d->m_backend = (prot == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(d->m_backend);
}

void DecryptTask::autodetectProtocolFromInput()
{
    if (!d->m_input) {
        return;
    }
    const Protocol p = findProtocol(d->m_input->classification());
    if (p == UnknownProtocol) {
        throw Exception(gpg_error(GPG_ERR_NOTHING_FOUND), i18n("Could not determine whether this was S/MIME- or OpenPGP-encrypted - maybe it is not ciphertext at all?"), Exception::MessageOnly);
    }
    setProtocol(p);
}

QString DecryptTask::label() const
{
    return i18n("Decrypting: %1...", d->m_input->label());
}

unsigned long long DecryptTask::inputSize() const
{
    return d->m_input ? d->m_input->size() : 0;
}

QString DecryptTask::inputLabel() const
{
    return d->m_input ? d->m_input->label() : QString();
}

QString DecryptTask::outputLabel() const
{
    return d->m_output ? d->m_output->label() : QString();
}

Protocol DecryptTask::protocol() const
{
    return d->m_protocol;
}

void DecryptTask::cancel()
{

}

void DecryptTask::doStart()
{
    kleo_assert(d->m_backend);

    try {
        QGpgME::DecryptJob *const job = d->m_backend->decryptJob();
        kleo_assert(job);
        d->registerJob(job);
        ensureIOOpen(d->m_input->ioDevice().get(), d->m_output->ioDevice().get());
        job->start(d->m_input->ioDevice(), d->m_output->ioDevice());
    } catch (const GpgME::Exception &e) {
        d->emitResult(fromDecryptResult(e.error(), QString::fromLocal8Bit(e.what()), AuditLog()));
    } catch (const std::exception &e) {
        d->emitResult(fromDecryptResult(make_error(GPG_ERR_INTERNAL), i18n("Caught exception: %1", QString::fromLocal8Bit(e.what())), AuditLog()));
    } catch (...) {
        d->emitResult(fromDecryptResult(make_error(GPG_ERR_INTERNAL), i18n("Caught unknown exception"), AuditLog()));
    }
}

class VerifyOpaqueTask::Private
{
    VerifyOpaqueTask *const q;
public:
    explicit Private(VerifyOpaqueTask *qq) : q(qq), m_backend(nullptr), m_protocol(UnknownProtocol)  {}

    void slotResult(const VerificationResult &, const QByteArray &);

    void registerJob(QGpgME::VerifyOpaqueJob *job)
    {
        q->connect(job, SIGNAL(result(GpgME::VerificationResult,QByteArray)),
                   q, SLOT(slotResult(GpgME::VerificationResult,QByteArray)));
        q->connect(job, SIGNAL(progress(QString,int,int)),
                   q, SLOT(setProgress(QString,int,int)));
    }

    void emitResult(const std::shared_ptr<DecryptVerifyResult> &result);

    std::shared_ptr<Input> m_input;
    std::shared_ptr<Output> m_output;
    const QGpgME::Protocol *m_backend;
    Protocol m_protocol;
};

void VerifyOpaqueTask::Private::emitResult(const std::shared_ptr<DecryptVerifyResult> &result)
{
    q->emitResult(result);
    Q_EMIT q->decryptVerifyResult(result);
}

void VerifyOpaqueTask::Private::slotResult(const VerificationResult &result, const QByteArray &plainText)
{
    updateKeys(result);
    {
        std::stringstream ss;
        ss << result;
        qCDebug(KLEOPATRA_LOG) << ss.str().c_str();
    }
    const AuditLog auditLog = auditLogFromSender(q->sender());
    if (result.error().code()) {
        m_output->cancel();
    } else {
        try {
            kleo_assert(!result.isNull());
            m_output->finalize();
        } catch (const GpgME::Exception &e) {
            emitResult(q->fromDecryptResult(e.error(), QString::fromLocal8Bit(e.what()), auditLog));
            return;
        } catch (const std::exception &e) {
            emitResult(q->fromDecryptResult(make_error(GPG_ERR_INTERNAL), i18n("Caught exception: %1", QString::fromLocal8Bit(e.what())), auditLog));
            return;
        } catch (...) {
            emitResult(q->fromDecryptResult(make_error(GPG_ERR_INTERNAL), i18n("Caught unknown exception"), auditLog));
            return;
        }
    }

    const int drErr = result.error().code();
    const QString errorString = m_output->errorString();
    if (((drErr == GPG_ERR_EIO || drErr == GPG_ERR_NO_DATA) && !errorString.isEmpty()) ||
          m_output->failed()) {
        emitResult(q->fromDecryptResult(result.error() ? result.error() : Error::fromCode(GPG_ERR_EIO),
                    errorString, auditLog));
        return;
    }

    emitResult(q->fromVerifyOpaqueResult(result, plainText, auditLog));
}

VerifyOpaqueTask::VerifyOpaqueTask(QObject *parent) : AbstractDecryptVerifyTask(parent), d(new Private(this))
{
}

VerifyOpaqueTask::~VerifyOpaqueTask()
{
}

void VerifyOpaqueTask::setInput(const std::shared_ptr<Input> &input)
{
    d->m_input = input;
    kleo_assert(d->m_input && d->m_input->ioDevice());
}

void VerifyOpaqueTask::setOutput(const std::shared_ptr<Output> &output)
{
    d->m_output = output;
    kleo_assert(d->m_output && d->m_output->ioDevice());
}

void VerifyOpaqueTask::setProtocol(Protocol prot)
{
    kleo_assert(prot != UnknownProtocol);
    d->m_protocol = prot;
    d->m_backend = (prot == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(d->m_backend);
}

void VerifyOpaqueTask::autodetectProtocolFromInput()
{
    if (!d->m_input) {
        return;
    }
    const Protocol p = findProtocol(d->m_input->classification());
    if (p == UnknownProtocol) {
        throw Exception(gpg_error(GPG_ERR_NOTHING_FOUND), i18n("Could not determine whether this is an S/MIME or an OpenPGP signature - maybe it is not a signature at all?"), Exception::MessageOnly);
    }
    setProtocol(p);
}

QString VerifyOpaqueTask::label() const
{
    return i18n("Verifying: %1...", d->m_input->label());
}

unsigned long long VerifyOpaqueTask::inputSize() const
{
    return d->m_input ? d->m_input->size() : 0;
}

QString VerifyOpaqueTask::inputLabel() const
{
    return d->m_input ? d->m_input->label() : QString();
}

QString VerifyOpaqueTask::outputLabel() const
{
    return d->m_output ? d->m_output->label() : QString();
}

Protocol VerifyOpaqueTask::protocol() const
{
    return d->m_protocol;
}

void VerifyOpaqueTask::cancel()
{

}

void VerifyOpaqueTask::doStart()
{
    kleo_assert(d->m_backend);

    try {
        QGpgME::VerifyOpaqueJob *const job = d->m_backend->verifyOpaqueJob();
        kleo_assert(job);
        d->registerJob(job);
        ensureIOOpen(d->m_input->ioDevice().get(), d->m_output ? d->m_output->ioDevice().get() : nullptr);
        job->start(d->m_input->ioDevice(), d->m_output ? d->m_output->ioDevice() : std::shared_ptr<QIODevice>());
    } catch (const GpgME::Exception &e) {
        d->emitResult(fromVerifyOpaqueResult(e.error(), QString::fromLocal8Bit(e.what()), AuditLog()));
    } catch (const std::exception &e) {
        d->emitResult(fromVerifyOpaqueResult(make_error(GPG_ERR_INTERNAL), i18n("Caught exception: %1", QString::fromLocal8Bit(e.what())), AuditLog()));
    } catch (...) {
        d->emitResult(fromVerifyOpaqueResult(make_error(GPG_ERR_INTERNAL), i18n("Caught unknown exception"), AuditLog()));
    }
}

class VerifyDetachedTask::Private
{
    VerifyDetachedTask *const q;
public:
    explicit Private(VerifyDetachedTask *qq) : q(qq), m_backend(nullptr), m_protocol(UnknownProtocol) {}

    void slotResult(const VerificationResult &);

    void registerJob(QGpgME::VerifyDetachedJob *job)
    {
        q->connect(job, SIGNAL(result(GpgME::VerificationResult)),
                   q, SLOT(slotResult(GpgME::VerificationResult)));
        q->connect(job, SIGNAL(progress(QString,int,int)),
                   q, SLOT(setProgress(QString,int,int)));
    }

    void emitResult(const std::shared_ptr<DecryptVerifyResult> &result);

    std::shared_ptr<Input> m_input, m_signedData;
    const QGpgME::Protocol *m_backend;
    Protocol m_protocol;
};

void VerifyDetachedTask::Private::emitResult(const std::shared_ptr<DecryptVerifyResult> &result)
{
    q->emitResult(result);
    Q_EMIT q->decryptVerifyResult(result);
}

void VerifyDetachedTask::Private::slotResult(const VerificationResult &result)
{
    updateKeys(result);
    {
        std::stringstream ss;
        ss << result;
        qCDebug(KLEOPATRA_LOG) << ss.str().c_str();
    }
    const AuditLog auditLog = auditLogFromSender(q->sender());
    try {
        kleo_assert(!result.isNull());
        emitResult(q->fromVerifyDetachedResult(result, auditLog));
    } catch (const GpgME::Exception &e) {
        emitResult(q->fromVerifyDetachedResult(e.error(), QString::fromLocal8Bit(e.what()), auditLog));
    } catch (const std::exception &e) {
        emitResult(q->fromVerifyDetachedResult(make_error(GPG_ERR_INTERNAL), i18n("Caught exception: %1", QString::fromLocal8Bit(e.what())), auditLog));
    } catch (...) {
        emitResult(q->fromVerifyDetachedResult(make_error(GPG_ERR_INTERNAL), i18n("Caught unknown exception"), auditLog));
    }
}

VerifyDetachedTask::VerifyDetachedTask(QObject *parent) : AbstractDecryptVerifyTask(parent), d(new Private(this))
{
}

VerifyDetachedTask::~VerifyDetachedTask()
{
}

void VerifyDetachedTask::setInput(const std::shared_ptr<Input> &input)
{
    d->m_input = input;
    kleo_assert(d->m_input && d->m_input->ioDevice());
}

void VerifyDetachedTask::setSignedData(const std::shared_ptr<Input> &signedData)
{
    d->m_signedData = signedData;
    kleo_assert(d->m_signedData && d->m_signedData->ioDevice());
}

void VerifyDetachedTask::setProtocol(Protocol prot)
{
    kleo_assert(prot != UnknownProtocol);
    d->m_protocol = prot;
    d->m_backend = (prot == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    kleo_assert(d->m_backend);
}

void VerifyDetachedTask::autodetectProtocolFromInput()
{
    if (!d->m_input) {
        return;
    }
    const Protocol p = findProtocol(d->m_input->classification());
    if (p == UnknownProtocol) {
        throw Exception(gpg_error(GPG_ERR_NOTHING_FOUND), i18n("Could not determine whether this is an S/MIME or an OpenPGP signature - maybe it is not a signature at all?"), Exception::MessageOnly);
    }
    setProtocol(p);
}

unsigned long long VerifyDetachedTask::inputSize() const
{
    return d->m_signedData ? d->m_signedData->size() : 0;
}

QString VerifyDetachedTask::label() const
{
    if (d->m_signedData) {
        return xi18nc("Verification of a detached signature in progress. The first file contains the data."
                      "The second file is the signature file.",
                      "Verifying: <filename>%1</filename> with <filename>%2</filename>...",
                      d->m_signedData->label(),
                      d->m_input->label());

    }
    return i18n("Verifying signature: %1...", d->m_input->label());
}

QString VerifyDetachedTask::inputLabel() const
{
    if (d->m_signedData && d->m_input) {
        return xi18nc("Verification of a detached signature summary. The first file contains the data."
                      "The second file is signature.",
                      "Verified <filename>%1</filename> with <filename>%2</filename>",
                      d->m_signedData->label(),
                      d->m_input->label());

    }
    return d->m_input ? d->m_input->label() : QString();
}

QString VerifyDetachedTask::outputLabel() const
{
    return QString();
}

Protocol VerifyDetachedTask::protocol() const
{
    return d->m_protocol;
}

void VerifyDetachedTask::cancel()
{

}

void VerifyDetachedTask::doStart()
{
    kleo_assert(d->m_backend);
    try {
        QGpgME::VerifyDetachedJob *const job = d->m_backend->verifyDetachedJob();
        kleo_assert(job);
        d->registerJob(job);
        ensureIOOpen(d->m_input->ioDevice().get(), nullptr);
        ensureIOOpen(d->m_signedData->ioDevice().get(), nullptr);
        job->start(d->m_input->ioDevice(), d->m_signedData->ioDevice());
    } catch (const GpgME::Exception &e) {
        d->emitResult(fromVerifyDetachedResult(e.error(), QString::fromLocal8Bit(e.what()), AuditLog()));
    } catch (const std::exception &e) {
        d->emitResult(fromVerifyDetachedResult(make_error(GPG_ERR_INTERNAL), i18n("Caught exception: %1", QString::fromLocal8Bit(e.what())), AuditLog()));
    } catch (...) {
        d->emitResult(fromVerifyDetachedResult(make_error(GPG_ERR_INTERNAL), i18n("Caught unknown exception"), AuditLog()));
    }
}

#include "moc_decryptverifytask.cpp"
