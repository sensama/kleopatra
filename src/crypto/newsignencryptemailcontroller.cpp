/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/newsignencryptemailcontroller.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009, 2010 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "newsignencryptemailcontroller.h"
#include "kleopatra_debug.h"
#include "encryptemailtask.h"
#include "signemailtask.h"
#include "taskcollection.h"
#include "sender.h"
#include "recipient.h"

#include "emailoperationspreferences.h"

#include <crypto/gui/signencryptemailconflictdialog.h>

#include "utils/input.h"
#include "utils/output.h"
#include <Libkleo/GnuPG>
#include "utils/kleo_assert.h"

#include <Libkleo/Stl_Util>
#include <Libkleo/KleoException>

#include <gpgme++/key.h>

#include <KMime/HeaderParsing>

#include <KLocalizedString>

#include <KMessageBox>

#include <QPointer>
#include <QTimer>

using namespace Kleo;
using namespace Kleo::Crypto;
using namespace Kleo::Crypto::Gui;
using namespace GpgME;
using namespace KMime::Types;

//
// BEGIN Conflict Detection
//

/*
  This code implements the following conflict detection algorithm:

  1. There is no conflict if and only if we have a Perfect Match.
  2. A Perfect Match is defined as:
    a. either a Perfect OpenPGP-Match and not even a Partial S/MIME Match
    b. or a Perfect S/MIME-Match and not even a Partial OpenPGP-Match
    c. or a Perfect OpenPGP-Match and preselected protocol=OpenPGP
    d. or a Perfect S/MIME-Match and preselected protocol=S/MIME
  3. For Protocol \in {OpenPGP,S/MIME}, a Perfect Protocol-Match is defined as:
    a. If signing, \foreach Sender, there is exactly one
         Matching Protocol-Certificate with
     i. can-sign=true
     ii. has-secret=true
    b. and, if encrypting, \foreach Recipient, there is exactly one
         Matching Protocol-Certificate with
     i. can-encrypt=true
     ii. (validity is not considered, cf. msg 24059)
  4. For Protocol \in {OpenPGP,S/MIME}, a Partial Protocol-Match is defined as:
    a. If signing, \foreach Sender, there is at least one
         Matching Protocol-Certificate with
     i. can-sign=true
     ii. has-secret=true
    b. and, if encrypting, \foreach Recipient, there is at least
     one Matching Protocol-Certificate with
     i. can-encrypt=true
     ii. (validity is not considered, cf. msg 24059)
  5. For Protocol \in {OpenPGP,S/MIME}, a Matching Protocol-Certificate is
     defined as matching by email-address. A revoked, disabled, or expired
     certificate is not considered a match.
  6. Sender is defined as those mailboxes that have been set with the SENDER
     command.
  7. Recipient is defined as those mailboxes that have been set with either the
     SENDER or the RECIPIENT commands.
*/

namespace
{

static size_t count_signing_certificates(Protocol proto, const Sender &sender)
{
    const size_t result = sender.signingCertificateCandidates(proto).size();
    qDebug("count_signing_certificates( %9s %20s ) == %2lu",
            proto == OpenPGP ? "OpenPGP," : proto == CMS ? "CMS," : "<unknown>,",
            qPrintable(sender.mailbox().prettyAddress()), result);
    return result;
}

static size_t count_encrypt_certificates(Protocol proto, const Sender &sender)
{
    const size_t result = sender.encryptToSelfCertificateCandidates(proto).size();
    qDebug("count_encrypt_certificates( %9s %20s ) == %2lu",
            proto == OpenPGP ? "OpenPGP," : proto == CMS ? "CMS," : "<unknown>,",
            qPrintable(sender.mailbox().prettyAddress()), result);
    return result;
}

static size_t count_encrypt_certificates(Protocol proto, const Recipient &recipient)
{        const size_t result = recipient.encryptionCertificateCandidates(proto).size();
    qDebug("count_encrypt_certificates( %9s %20s ) == %2lu",
            proto == OpenPGP ? "OpenPGP," : proto == CMS ? "CMS," : "<unknown>,",
            qPrintable(recipient.mailbox().prettyAddress()), result);
    return result;
}

}

static bool has_perfect_match(bool sign, bool encrypt, Protocol proto, const std::vector<Sender> &senders, const std::vector<Recipient> &recipients)
{
    if (sign)
        if (!std::all_of(senders.cbegin(), senders.cend(),
                         [proto](const Sender &sender) { return count_signing_certificates(proto, sender) == 1; })) {
            return false;
        }
    if (encrypt)
        if (!std::all_of(senders.cbegin(), senders.cend(),
                         [proto](const Sender &sender) { return count_encrypt_certificates(proto, sender) == 1; })
            || !std::all_of(recipients.cbegin(), recipients.cend(),
                           [proto](const Recipient &rec) { return count_encrypt_certificates(proto, rec) == 1; })) {
            return false;
        }
    return true;
}

static bool has_partial_match(bool sign, bool encrypt, Protocol proto, const std::vector<Sender> &senders, const std::vector<Recipient> &recipients)
{
    if (sign)
        if (std::all_of(senders.cbegin(), senders.cend(),
                        [proto](const Sender &sender) { return count_signing_certificates(proto, sender) >= 1; })) {
            return false;
        }
    if (encrypt)
        if (!std::all_of(senders.cbegin(), senders.cend(),
                         [proto](const Sender &sender) { return count_encrypt_certificates(proto, sender) >= 1; })
            || !std::all_of(recipients.cbegin(), recipients.cend(),
                            [proto](const Recipient &rec) { return count_encrypt_certificates(proto, rec) >= 1; })) {
            return false;
        }
    return true;
}

static bool has_perfect_overall_match(bool sign, bool encrypt, const std::vector<Sender> &senders, const std::vector<Recipient> &recipients, Protocol presetProtocol)
{
    return (presetProtocol == OpenPGP   &&   has_perfect_match(sign, encrypt, OpenPGP, senders, recipients))
           || (presetProtocol == CMS    &&   has_perfect_match(sign, encrypt, CMS,     senders, recipients))
           || (has_perfect_match(sign, encrypt, OpenPGP, senders, recipients)   &&   !has_partial_match(sign, encrypt, CMS,     senders, recipients))
           || (has_perfect_match(sign, encrypt, CMS,     senders, recipients)   &&   !has_partial_match(sign, encrypt, OpenPGP, senders, recipients));
}

static bool has_conflict(bool sign, bool encrypt, const std::vector<Sender> &senders, const std::vector<Recipient> &recipients, Protocol presetProtocol)
{
    return !has_perfect_overall_match(sign, encrypt, senders, recipients, presetProtocol);
}

static bool is_de_vs_compliant(bool sign, bool encrypt, const std::vector<Sender> &senders, const std::vector<Recipient> &recipients, Protocol presetProtocol)
{
    if (presetProtocol == Protocol::UnknownProtocol) {
        return false;
    }
    if (sign) {
        for (const auto &sender: senders) {
            const auto &key = sender.resolvedSigningKey(presetProtocol);
            if (!key.isDeVs() || keyValidity(key) < GpgME::UserID::Validity::Full) {
                return false;
            }
        }
    }

    if (encrypt) {
        for (const auto &sender: senders) {
            const auto &key = sender.resolvedSigningKey(presetProtocol);
            if (!key.isDeVs() || keyValidity(key) < GpgME::UserID::Validity::Full) {
                return false;
            }
        }

        for (const auto &recipient: recipients) {
            const auto &key = recipient.resolvedEncryptionKey(presetProtocol);
            if (!key.isDeVs() || keyValidity(key) < GpgME::UserID::Validity::Full) {
                return false;
            }
        }
    }

    return true;
}

//
// END Conflict Detection
//

static std::vector<Sender> mailbox2sender(const std::vector<Mailbox> &mbs)
{
    std::vector<Sender> senders;
    senders.reserve(mbs.size());
    for (const Mailbox &mb : mbs) {
        senders.push_back(Sender(mb));
    }
    return senders;
}

static std::vector<Recipient> mailbox2recipient(const std::vector<Mailbox> &mbs)
{
    std::vector<Recipient> recipients;
    recipients.reserve(mbs.size());
    for (const Mailbox &mb : mbs) {
        recipients.push_back(Recipient(mb));
    }
    return recipients;
}

class NewSignEncryptEMailController::Private
{
    friend class ::Kleo::Crypto::NewSignEncryptEMailController;
    NewSignEncryptEMailController *const q;
public:
    explicit Private(NewSignEncryptEMailController *qq);
    ~Private();

private:
    void slotDialogAccepted();
    void slotDialogRejected();

private:
    void ensureDialogVisible();
    void cancelAllTasks();

    void startSigning();
    void startEncryption();
    void schedule();
    std::shared_ptr<Task> takeRunnable(GpgME::Protocol proto);

private:
    bool sign : 1;
    bool encrypt : 1;
    bool resolvingInProgress : 1;
    bool certificatesResolved : 1;
    bool detached : 1;
    Protocol presetProtocol;
    std::vector<Key> signers, recipients;
    std::vector< std::shared_ptr<Task> > runnable, completed;
    std::shared_ptr<Task> cms, openpgp;
    QPointer<SignEncryptEMailConflictDialog> dialog;
};

NewSignEncryptEMailController::Private::Private(NewSignEncryptEMailController *qq)
    : q(qq),
      sign(false),
      encrypt(false),
      resolvingInProgress(false),
      certificatesResolved(false),
      detached(false),
      presetProtocol(UnknownProtocol),
      signers(),
      recipients(),
      runnable(),
      cms(),
      openpgp(),
      dialog(new SignEncryptEMailConflictDialog)
{
    connect(dialog, SIGNAL(accepted()), q, SLOT(slotDialogAccepted()));
    connect(dialog, SIGNAL(rejected()), q, SLOT(slotDialogRejected()));
}

NewSignEncryptEMailController::Private::~Private()
{
    delete dialog;
}

NewSignEncryptEMailController::NewSignEncryptEMailController(const std::shared_ptr<ExecutionContext> &xc, QObject *p)
    : Controller(xc, p), d(new Private(this))
{

}

NewSignEncryptEMailController::NewSignEncryptEMailController(QObject *p)
    : Controller(p), d(new Private(this))
{

}

NewSignEncryptEMailController::~NewSignEncryptEMailController()
{
    qCDebug(KLEOPATRA_LOG);
}

void NewSignEncryptEMailController::setSubject(const QString &subject)
{
    d->dialog->setSubject(subject);
}

void NewSignEncryptEMailController::setProtocol(Protocol proto)
{
    d->presetProtocol = proto;
    d->dialog->setPresetProtocol(proto);
}

Protocol NewSignEncryptEMailController::protocol() const
{
    return d->dialog->selectedProtocol();
}

const char *NewSignEncryptEMailController::protocolAsString() const
{
    switch (protocol()) {
    case OpenPGP: return "OpenPGP";
    case CMS:     return "CMS";
    default:
        throw Kleo::Exception(gpg_error(GPG_ERR_INTERNAL),
                              i18n("Call to NewSignEncryptEMailController::protocolAsString() is ambiguous."));
    }
}

void NewSignEncryptEMailController::setSigning(bool sign)
{
    d->sign = sign;
    d->dialog->setSign(sign);
}

bool NewSignEncryptEMailController::isSigning() const
{
    return d->sign;
}

void NewSignEncryptEMailController::setEncrypting(bool encrypt)
{
    d->encrypt = encrypt;
    d->dialog->setEncrypt(encrypt);
}

bool NewSignEncryptEMailController::isEncrypting() const
{
    return d->encrypt;
}

void NewSignEncryptEMailController::setDetachedSignature(bool detached)
{
    d->detached = detached;
}

bool NewSignEncryptEMailController::isResolvingInProgress() const
{
    return d->resolvingInProgress;
}

bool NewSignEncryptEMailController::areCertificatesResolved() const
{
    return d->certificatesResolved;
}

static bool is_dialog_quick_mode(bool sign, bool encrypt)
{
    const EMailOperationsPreferences prefs;
    return (!sign    || prefs.quickSignEMail())
           && (!encrypt || prefs.quickEncryptEMail())
           ;
}

static void save_dialog_quick_mode(bool on)
{
    EMailOperationsPreferences prefs;
    prefs.setQuickSignEMail(on);
    prefs.setQuickEncryptEMail(on);
    prefs.save();
}

void NewSignEncryptEMailController::startResolveCertificates(const std::vector<Mailbox> &r, const std::vector<Mailbox> &s)
{
    d->certificatesResolved = false;
    d->resolvingInProgress = true;

    const std::vector<Sender> senders = mailbox2sender(s);
    const std::vector<Recipient> recipients = mailbox2recipient(r);
    const bool quickMode = is_dialog_quick_mode(d->sign, d->encrypt);

    const bool conflict = quickMode && has_conflict(d->sign, d->encrypt, senders, recipients, d->presetProtocol);

    d->dialog->setQuickMode(quickMode);
    d->dialog->setSenders(senders);
    d->dialog->setRecipients(recipients);
    d->dialog->pickProtocol();
    d->dialog->setConflict(conflict);

    const bool compliant = !Kleo::gnupgUsesDeVsCompliance() ||
                           (Kleo::gnupgIsDeVsCompliant() && is_de_vs_compliant(d->sign,
                                                                               d->encrypt,
                                                                               senders,
                                                                               recipients,
                                                                               d->presetProtocol));

    if (quickMode && !conflict && compliant) {
        QMetaObject::invokeMethod(this, "slotDialogAccepted", Qt::QueuedConnection);
    } else {
        d->ensureDialogVisible();
    }
}

void NewSignEncryptEMailController::Private::slotDialogAccepted()
{
    if (dialog->isQuickMode() != is_dialog_quick_mode(sign, encrypt)) {
        save_dialog_quick_mode(dialog->isQuickMode());
    }
    resolvingInProgress = false;
    certificatesResolved = true;
    signers = dialog->resolvedSigningKeys();
    recipients = dialog->resolvedEncryptionKeys();
    QMetaObject::invokeMethod(q, "certificatesResolved", Qt::QueuedConnection);
}

void NewSignEncryptEMailController::Private::slotDialogRejected()
{
    resolvingInProgress = false;
    certificatesResolved = false;
    QMetaObject::invokeMethod(q, "error", Qt::QueuedConnection,
                              Q_ARG(int, gpg_error(GPG_ERR_CANCELED)),
                              Q_ARG(QString, i18n("User cancel")));
}

void NewSignEncryptEMailController::startEncryption(const std::vector< std::shared_ptr<Input> > &inputs, const std::vector< std::shared_ptr<Output> > &outputs)
{

    kleo_assert(d->encrypt);
    kleo_assert(!d->resolvingInProgress);

    kleo_assert(!inputs.empty());
    kleo_assert(outputs.size() == inputs.size());

    std::vector< std::shared_ptr<Task> > tasks;
    tasks.reserve(inputs.size());

    kleo_assert(!d->recipients.empty());

    for (unsigned int i = 0, end = inputs.size(); i < end; ++i) {

        const std::shared_ptr<EncryptEMailTask> task(new EncryptEMailTask);

        task->setInput(inputs[i]);
        task->setOutput(outputs[i]);
        task->setRecipients(d->recipients);

        tasks.push_back(task);
    }

    // append to runnable stack
    d->runnable.insert(d->runnable.end(), tasks.begin(), tasks.end());

    d->startEncryption();
}

void NewSignEncryptEMailController::Private::startEncryption()
{
    std::shared_ptr<TaskCollection> coll(new TaskCollection);
    std::vector<std::shared_ptr<Task> > tmp;
    tmp.reserve(runnable.size());
    std::copy(runnable.cbegin(), runnable.cend(), std::back_inserter(tmp));
    coll->setTasks(tmp);
#if 0
#warning use a new result dialog
    // ### use a new result dialog
    dialog->setTaskCollection(coll);
#endif
    for (const std::shared_ptr<Task> &t : std::as_const(tmp)) {
        q->connectTask(t);
    }
    schedule();
}

void NewSignEncryptEMailController::startSigning(const std::vector< std::shared_ptr<Input> > &inputs, const std::vector< std::shared_ptr<Output> > &outputs)
{

    kleo_assert(d->sign);
    kleo_assert(!d->resolvingInProgress);

    kleo_assert(!inputs.empty());
    kleo_assert(!outputs.empty());

    std::vector< std::shared_ptr<Task> > tasks;
    tasks.reserve(inputs.size());

    kleo_assert(!d->signers.empty());
    kleo_assert(std::none_of(d->signers.cbegin(), d->signers.cend(), std::mem_fn(&Key::isNull)));

    for (unsigned int i = 0, end = inputs.size(); i < end; ++i) {

        const std::shared_ptr<SignEMailTask> task(new SignEMailTask);

        task->setInput(inputs[i]);
        task->setOutput(outputs[i]);
        task->setSigners(d->signers);
        task->setDetachedSignature(d->detached);

        tasks.push_back(task);
    }

    // append to runnable stack
    d->runnable.insert(d->runnable.end(), tasks.begin(), tasks.end());

    d->startSigning();
}

void NewSignEncryptEMailController::Private::startSigning()
{
    std::shared_ptr<TaskCollection> coll(new TaskCollection);
    std::vector<std::shared_ptr<Task> > tmp;
    tmp.reserve(runnable.size());
    std::copy(runnable.cbegin(), runnable.cend(), std::back_inserter(tmp));
    coll->setTasks(tmp);
#if 0
#warning use a new result dialog
    // ### use a new result dialog
    dialog->setTaskCollection(coll);
#endif
    for (const std::shared_ptr<Task> &t : std::as_const(tmp)) {
        q->connectTask(t);
    }
    schedule();
}

void NewSignEncryptEMailController::Private::schedule()
{

    if (!cms)
        if (const std::shared_ptr<Task> t = takeRunnable(CMS)) {
            t->start();
            cms = t;
        }

    if (!openpgp)
        if (const std::shared_ptr<Task> t = takeRunnable(OpenPGP)) {
            t->start();
            openpgp = t;
        }

    if (cms || openpgp) {
        return;
    }
    kleo_assert(runnable.empty());
    q->emitDoneOrError();
}

std::shared_ptr<Task> NewSignEncryptEMailController::Private::takeRunnable(GpgME::Protocol proto)
{
    const auto it = std::find_if(runnable.begin(), runnable.end(),
                                 [proto](const std::shared_ptr<Task> &task) { return task->protocol() == proto; });
    if (it == runnable.end()) {
        return std::shared_ptr<Task>();
    }

    const std::shared_ptr<Task> result = *it;
    runnable.erase(it);
    return result;
}

void NewSignEncryptEMailController::doTaskDone(const Task *task, const std::shared_ptr<const Task::Result> &result)
{
    Q_ASSERT(task);

    if (result && result->hasError()) {
        QPointer<QObject> that = this;
        if (result->details().isEmpty())
            KMessageBox::        sorry(nullptr,
                                       result->overview(),
                                       i18nc("@title:window", "Error"));
        else
            KMessageBox::detailedSorry(nullptr,
                                       result->overview(),
                                       result->details(),
                                       i18nc("@title:window", "Error"));
        if (!that) {
            return;
        }
    }

    // We could just delete the tasks here, but we can't use
    // Qt::QueuedConnection here (we need sender()) and other slots
    // might not yet have executed. Therefore, we push completed tasks
    // into a burial container

    if (task == d->cms.get()) {
        d->completed.push_back(d->cms);
        d->cms.reset();
    } else if (task == d->openpgp.get()) {
        d->completed.push_back(d->openpgp);
        d->openpgp.reset();
    }

    QTimer::singleShot(0, this, SLOT(schedule()));
}

void NewSignEncryptEMailController::cancel()
{
    try {
        d->dialog->close();
        d->cancelAllTasks();
    } catch (const std::exception &e) {
        qCDebug(KLEOPATRA_LOG) << "Caught exception: " << e.what();
    }
}

void NewSignEncryptEMailController::Private::cancelAllTasks()
{

    // we just kill all runnable tasks - this will not result in
    // signal emissions.
    runnable.clear();

    // a cancel() will result in a call to
    if (cms) {
        cms->cancel();
    }
    if (openpgp) {
        openpgp->cancel();
    }
}

void NewSignEncryptEMailController::Private::ensureDialogVisible()
{
    q->bringToForeground(dialog, true);
}

#include "moc_newsignencryptemailcontroller.cpp"
