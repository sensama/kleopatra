/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/signencryptfilescontroller.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB
    2017 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

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

#include "signencryptfilescontroller.h"

#include "signencrypttask.h"
#include "certificateresolver.h"

#include "crypto/gui/signencryptfileswizard.h"
#include "crypto/taskcollection.h"

#include "fileoperationspreferences.h"

#include "utils/input.h"
#include "utils/output.h"
#include "utils/kleo_assert.h"
#include "utils/archivedefinition.h"
#include "utils/path-helper.h"

#include <Libkleo/Exception>
#include <Libkleo/Classify>

#include <kmime/kmime_header_parsing.h>

#include <KLocalizedString>
#include "kleopatra_debug.h"

#include <QPointer>
#include <QTimer>
#include <QFileInfo>
#include <QDir>

using namespace Kleo;
using namespace Kleo::Crypto;
using namespace GpgME;
using namespace KMime::Types;

class SignEncryptFilesController::Private
{
    friend class ::Kleo::Crypto::SignEncryptFilesController;
    SignEncryptFilesController *const q;
public:
    explicit Private(SignEncryptFilesController *qq);
    ~Private();

private:
    void slotWizardOperationPrepared();
    void slotWizardCanceled();

private:
    void ensureWizardCreated();
    void ensureWizardVisible();
    void updateWizardMode();
    void cancelAllTasks();
    void reportError(int err, const QString &details)
    {
        q->setLastError(err, details);
        q->emitDoneOrError();
    }

    void schedule();
    std::shared_ptr<SignEncryptTask> takeRunnable(GpgME::Protocol proto);

    static void assertValidOperation(unsigned int);
    static QString titleForOperation(unsigned int op);
private:
    std::vector< std::shared_ptr<SignEncryptTask> > runnable, completed;
    std::shared_ptr<SignEncryptTask> cms, openpgp;
    QPointer<SignEncryptFilesWizard> wizard;
    QStringList files;
    unsigned int operation;
    Protocol protocol;
};

SignEncryptFilesController::Private::Private(SignEncryptFilesController *qq)
    : q(qq),
      runnable(),
      cms(),
      openpgp(),
      wizard(),
      files(),
      operation(SignAllowed | EncryptAllowed | ArchiveAllowed),
      protocol(UnknownProtocol)
{

}

SignEncryptFilesController::Private::~Private()
{
    qCDebug(KLEOPATRA_LOG);
}

QString SignEncryptFilesController::Private::titleForOperation(unsigned int op)
{
    const bool signDisallowed = (op & SignMask) == SignDisallowed;
    const bool encryptDisallowed = (op & EncryptMask) == EncryptDisallowed;
    const bool archiveSelected = (op & ArchiveMask) == ArchiveForced;

    kleo_assert(!signDisallowed || !encryptDisallowed);

    if (!signDisallowed && encryptDisallowed) {
        if (archiveSelected) {
            return i18n("Archive and Sign Files");
        } else {
            return i18n("Sign Files");
        }
    }

    if (signDisallowed && !encryptDisallowed) {
        if (archiveSelected) {
            return i18n("Archive and Encrypt Files");
        } else {
            return i18n("Encrypt Files");
        }
    }

    if (archiveSelected) {
        return i18n("Archive and Sign/Encrypt Files");
    } else {
        return i18n("Sign/Encrypt Files");
    }
}

SignEncryptFilesController::SignEncryptFilesController(QObject *p)
    : Controller(p), d(new Private(this))
{

}

SignEncryptFilesController::SignEncryptFilesController(const std::shared_ptr<const ExecutionContext> &ctx, QObject *p)
    : Controller(ctx, p), d(new Private(this))
{

}

SignEncryptFilesController::~SignEncryptFilesController()
{
    qCDebug(KLEOPATRA_LOG);
    if (d->wizard && !d->wizard->isVisible()) {
        delete d->wizard;
    }
    //d->wizard->close(); ### ?
}

void SignEncryptFilesController::setProtocol(Protocol proto)
{
    kleo_assert(d->protocol == UnknownProtocol ||
                d->protocol == proto);
    d->protocol = proto;
    d->ensureWizardCreated();
}

Protocol SignEncryptFilesController::protocol() const
{
    return d->protocol;
}

// static
void SignEncryptFilesController::Private::assertValidOperation(unsigned int op)
{
    kleo_assert((op & SignMask)    == SignDisallowed    ||
                (op & SignMask)    == SignAllowed       ||
                (op & SignMask)    == SignSelected);
    kleo_assert((op & EncryptMask) == EncryptDisallowed ||
                (op & EncryptMask) == EncryptAllowed    ||
                (op & EncryptMask) == EncryptSelected);
    kleo_assert((op & ArchiveMask) == ArchiveDisallowed ||
                (op & ArchiveMask) == ArchiveAllowed    ||
                (op & ArchiveMask) == ArchiveForced);
    kleo_assert((op & ~(SignMask | EncryptMask | ArchiveMask)) == 0);
}

void SignEncryptFilesController::setOperationMode(unsigned int mode)
{
    Private::assertValidOperation(mode);
    d->operation = mode;
    d->updateWizardMode();
}

void SignEncryptFilesController::Private::updateWizardMode()
{
    if (!wizard) {
        return;
    }
    wizard->setWindowTitle(titleForOperation(operation));
    const unsigned int signOp = (operation & SignMask);
    const unsigned int encrOp = (operation & EncryptMask);
    const unsigned int archOp = (operation & ArchiveMask);

    if (signOp == SignDisallowed) {
        wizard->setSigningUserMutable(false);
        wizard->setSigningPreset(false);
    } else {
        wizard->setSigningUserMutable(true);
        wizard->setSigningPreset(signOp == SignSelected);
    }

    if (encrOp == EncryptDisallowed) {
        wizard->setEncryptionPreset(false);
        wizard->setEncryptionUserMutable(false);
    } else {
        wizard->setEncryptionUserMutable(true);
        wizard->setEncryptionPreset(false);
        wizard->setEncryptionPreset(encrOp == EncryptSelected);
    }

    wizard->setArchiveForced(archOp == ArchiveForced);
    wizard->setArchiveMutable(archOp == ArchiveAllowed);
}

unsigned int SignEncryptFilesController::operationMode() const
{
    return d->operation;
}

static const char *extension(bool pgp, bool sign, bool encrypt, bool ascii, bool detached)
{
    unsigned int cls = pgp ? Class::OpenPGP : Class::CMS;
    if (encrypt) {
        cls |= Class::CipherText;
    } else if (sign) {
        cls |= detached ? Class::DetachedSignature : Class::OpaqueSignature;
    }
    cls |= ascii ? Class::Ascii : Class::Binary;
    const bool usePGPFileExt = FileOperationsPreferences().usePGPFileExt();
    if (const char *const ext = outputFileExtension(cls, usePGPFileExt)) {
        return ext;
    } else {
        return "out";
    }
}

static std::shared_ptr<ArchiveDefinition> getDefaultAd()
{
    std::vector<std::shared_ptr<ArchiveDefinition> > ads = ArchiveDefinition::getArchiveDefinitions();
    Q_ASSERT(!ads.empty());
    std::shared_ptr<ArchiveDefinition> ad = ads.front();
    const FileOperationsPreferences prefs;
    Q_FOREACH (const std::shared_ptr<ArchiveDefinition> toCheck, ads) {
        if (toCheck->id() == prefs.archiveCommand()) {
            ad = toCheck;
            break;
        }
    }
    return ad;
}

static QMap <int, QString> buildOutputNames(const QStringList &files, const bool archive)
{
    QMap <int, QString> nameMap;

    // Build the default names for the wizard.
    QString baseNameCms;
    QString baseNamePgp;
    const QFileInfo firstFile(files.first());
    if (archive) {
        QString baseName;
        baseName = QDir(heuristicBaseDirectory(files)).absoluteFilePath(files.size() > 1 ?
                i18nc("base name of an archive file, e.g. archive.zip or archive.tar.gz", "archive") :
                firstFile.baseName());

        const auto ad = getDefaultAd();
        baseNamePgp = baseName + QLatin1Char('.') + ad->extensions(GpgME::OpenPGP).first() + QLatin1Char('.');
        baseNameCms = baseName + QLatin1Char('.') + ad->extensions(GpgME::CMS).first() + QLatin1Char('.');
    } else {
        baseNameCms = baseNamePgp = files.first() + QLatin1Char('.');
    }
    const FileOperationsPreferences prefs;
    const bool ascii = prefs.addASCIIArmor();

    nameMap.insert(SignEncryptFilesWizard::SignatureCMS, baseNameCms + QString::fromLatin1(extension(false, true, false, ascii, true)));
    nameMap.insert(SignEncryptFilesWizard::EncryptedCMS, baseNameCms + QString::fromLatin1(extension(false, false, true, ascii, false)));
    nameMap.insert(SignEncryptFilesWizard::CombinedPGP,  baseNamePgp + QString::fromLatin1(extension(true, true, true, ascii, false)));
    nameMap.insert(SignEncryptFilesWizard::EncryptedPGP, baseNamePgp + QString::fromLatin1(extension(true, false, true, ascii, false)));
    nameMap.insert(SignEncryptFilesWizard::SignaturePGP, baseNamePgp + QString::fromLatin1(extension(true, true, false, ascii, true)));
    nameMap.insert(SignEncryptFilesWizard::Directory, heuristicBaseDirectory(files));
    return nameMap;
}

static QMap <int, QString> buildOutputNamesForDir(const QString &file, const QMap <int, QString> &orig)
{
    QMap <int, QString> ret;

    const QString dir = orig.value(SignEncryptFilesWizard::Directory);
    if (dir.isEmpty()) {
        return orig;
    }

    // Build the default names for the wizard.
    const QFileInfo fi(file);
    const QString baseName = dir + QLatin1Char('/') + fi.fileName() + QLatin1Char('.');

    const FileOperationsPreferences prefs;
    const bool ascii = prefs.addASCIIArmor();

    ret.insert(SignEncryptFilesWizard::SignatureCMS, baseName + QString::fromLatin1(extension(false, true, false, ascii, true)));
    ret.insert(SignEncryptFilesWizard::EncryptedCMS, baseName + QString::fromLatin1(extension(false, false, true, ascii, false)));
    ret.insert(SignEncryptFilesWizard::CombinedPGP,  baseName + QString::fromLatin1(extension(true, true, true, ascii, false)));
    ret.insert(SignEncryptFilesWizard::EncryptedPGP, baseName + QString::fromLatin1(extension(true, false, true, ascii, false)));
    ret.insert(SignEncryptFilesWizard::SignaturePGP, baseName + QString::fromLatin1(extension(true, true, false, ascii, true)));
    return ret;
}

void SignEncryptFilesController::setFiles(const QStringList &files)
{
    kleo_assert(!files.empty());
    d->files = files;
    bool archive = false;

    if (files.size() > 1) {
        setOperationMode((operationMode() & ~ArchiveMask) | ArchiveAllowed);
        archive = true;
    }
    for (const auto &file: files) {
        if (QFileInfo(file).isDir()) {
            setOperationMode((operationMode() & ~ArchiveMask) | ArchiveForced);
            archive = true;
            break;
        }
    }
    d->ensureWizardCreated();
    d->wizard->setOutputNames(buildOutputNames(files, archive));
}

void SignEncryptFilesController::Private::slotWizardCanceled()
{
    qCDebug(KLEOPATRA_LOG);
    reportError(gpg_error(GPG_ERR_CANCELED), i18n("User cancel"));
}

void SignEncryptFilesController::start()
{
    d->ensureWizardVisible();
}

static std::shared_ptr<SignEncryptTask>
createSignEncryptTaskForFileInfo(const QFileInfo &fi, bool ascii,
                                 const std::vector<Key> &recipients, const std::vector<Key> &signers,
                                 const QString &outputName, bool symmetric)
{
    const std::shared_ptr<SignEncryptTask> task(new SignEncryptTask);
    Q_ASSERT(!signers.empty() || !recipients.empty() || symmetric);
    task->setAsciiArmor(ascii);
    if (!signers.empty()) {
        task->setSign(true);
        task->setSigners(signers);
        task->setDetachedSignature(true);
    } else {
        task->setSign(false);
    }
    if (!recipients.empty()) {
        task->setEncrypt(true);
        task->setRecipients(recipients);
        task->setDetachedSignature(false);
    } else {
        task->setEncrypt(false);
    }
    task->setEncryptSymmetric(symmetric);
    const QString input = fi.absoluteFilePath();
    task->setInputFileName(input);
    task->setInput(Input::createFromFile(input));

    task->setOutputFileName(outputName);

    return task;
}

static std::shared_ptr<SignEncryptTask>
createArchiveSignEncryptTaskForFiles(const QStringList &files,
                                     const std::shared_ptr<ArchiveDefinition> &ad, bool pgp, bool ascii,
                                     const std::vector<Key> &recipients, const std::vector<Key> &signers,
                                     const QString& outputName, bool symmetric)
{
    const std::shared_ptr<SignEncryptTask> task(new SignEncryptTask);
    task->setEncryptSymmetric(symmetric);
    Q_ASSERT(!signers.empty() || !recipients.empty() || symmetric);
    task->setAsciiArmor(ascii);
    if (!signers.empty()) {
        task->setSign(true);
        task->setSigners(signers);
        task->setDetachedSignature(false);
    } else {
        task->setSign(false);
    }
    if (!recipients.empty()) {
        task->setEncrypt(true);
        task->setRecipients(recipients);
    } else {
        task->setEncrypt(false);
    }

    kleo_assert(ad);

    const Protocol proto = pgp ? OpenPGP : CMS;

    task->setInputFileNames(files);
    task->setInput(ad->createInputFromPackCommand(proto, files));

    task->setOutputFileName(outputName);

    return task;
}

static std::vector< std::shared_ptr<SignEncryptTask> >
createSignEncryptTasksForFileInfo(const QFileInfo &fi, bool ascii, const std::vector<Key> &pgpRecipients, const std::vector<Key> &pgpSigners,
                                  const std::vector<Key> &cmsRecipients, const std::vector<Key> &cmsSigners, const QMap<int, QString> &outputNames,
                                  bool symmetric)
{
    std::vector< std::shared_ptr<SignEncryptTask> > result;

    const bool pgp = !pgpSigners.empty() || !pgpRecipients.empty();

    const bool cms = !cmsSigners.empty() || !cmsRecipients.empty();

    result.reserve(pgp + cms);


    if (pgp || symmetric) {
        // Symmetric encryption is only supported for PGP
        int outKind = 0;
        if ((!pgpRecipients.empty() || symmetric)&& !pgpSigners.empty()) {
            outKind = SignEncryptFilesWizard::CombinedPGP;
        } else if (!pgpRecipients.empty() || symmetric) {
            outKind = SignEncryptFilesWizard::EncryptedPGP;
        } else {
            outKind = SignEncryptFilesWizard::SignaturePGP;
        }
        result.push_back(createSignEncryptTaskForFileInfo(fi, ascii, pgpRecipients, pgpSigners, outputNames[outKind], symmetric));
    }
    if (cms) {
        // There is no combined sign / encrypt in gpgsm so we create one sign task
        // and one encrypt task. Which leaves us with the age old dilemma, encrypt
        // then sign, or sign then encrypt. Ugly.
        if (!cmsSigners.empty()) {
            result.push_back(createSignEncryptTaskForFileInfo(fi, ascii, std::vector<Key>(),
                                                              cmsSigners, outputNames[SignEncryptFilesWizard::SignatureCMS],
                                                              false));
        }
        if (!cmsRecipients.empty()) {
            result.push_back(createSignEncryptTaskForFileInfo(fi, ascii, cmsRecipients,
                                                              std::vector<Key>(), outputNames[SignEncryptFilesWizard::EncryptedCMS],
                                                              false));
        }
    }

    return result;
}

static std::vector< std::shared_ptr<SignEncryptTask> >
createArchiveSignEncryptTasksForFiles(const QStringList &files, const std::shared_ptr<ArchiveDefinition> &ad,
                                      bool ascii, const std::vector<Key> &pgpRecipients,
                                      const std::vector<Key> &pgpSigners, const std::vector<Key> &cmsRecipients, const std::vector<Key> &cmsSigners,
                                      const QMap<int, QString> &outputNames, bool symmetric)
{
    std::vector< std::shared_ptr<SignEncryptTask> > result;

    const bool pgp = !pgpSigners.empty() || !pgpRecipients.empty();

    const bool cms = !cmsSigners.empty() || !cmsRecipients.empty();

    result.reserve(pgp + cms);

    if (pgp || symmetric) {
        int outKind = 0;
        if ((!pgpRecipients.empty() || symmetric) && !pgpSigners.empty()) {
            outKind = SignEncryptFilesWizard::CombinedPGP;
        } else if (!pgpRecipients.empty() || symmetric) {
            outKind = SignEncryptFilesWizard::EncryptedPGP;
        } else {
            outKind = SignEncryptFilesWizard::SignaturePGP;
        }
        result.push_back(createArchiveSignEncryptTaskForFiles(files, ad, true,  ascii, pgpRecipients, pgpSigners, outputNames[outKind], symmetric));
    }
    if (cms) {
        if (!cmsSigners.empty()) {
            result.push_back(createArchiveSignEncryptTaskForFiles(files, ad, false, ascii,
                                                                  std::vector<Key>(), cmsSigners, outputNames[SignEncryptFilesWizard::SignatureCMS],
                                                                  false));
        }
        if (!cmsRecipients.empty()) {
            result.push_back(createArchiveSignEncryptTaskForFiles(files, ad, false, ascii,
                                                                  cmsRecipients, std::vector<Key>(), outputNames[SignEncryptFilesWizard::EncryptedCMS],
                                                                  false));
        }
    }

    return result;
}

void SignEncryptFilesController::Private::slotWizardOperationPrepared()
{

    try {
        kleo_assert(wizard);
        kleo_assert(!files.empty());

        const bool archive = (wizard->outputNames().value(SignEncryptFilesWizard::Directory).isNull() && files.size() > 1) ||
                             ((operation & ArchiveMask) == ArchiveForced);

        const QVector<Key> recipients = wizard->resolvedRecipients();
        const QVector<Key> signers = wizard->resolvedSigners();

        const FileOperationsPreferences prefs;
        const bool ascii = prefs.addASCIIArmor();

        QVector<Key> pgpRecipients, cmsRecipients, pgpSigners, cmsSigners;
        Q_FOREACH (const Key &k, recipients) {
            if (k.protocol() == GpgME::OpenPGP) {
                pgpRecipients << k;
            } else {
                cmsRecipients << k;
            }
        }

        Q_FOREACH (const Key &k, signers) {
            if (k.protocol() == GpgME::OpenPGP) {
                pgpSigners << k;
            } else {
                cmsSigners << k;
            }
        }

        std::vector< std::shared_ptr<SignEncryptTask> > tasks;
        if (!archive) {
            tasks.reserve(files.size());
        }

        if (archive) {
            tasks = createArchiveSignEncryptTasksForFiles(files,
                    getDefaultAd(),
                    ascii,
                    pgpRecipients.toStdVector(),
                    pgpSigners.toStdVector(),
                    cmsRecipients.toStdVector(),
                    cmsSigners.toStdVector(),
                    wizard->outputNames(),
                    wizard->encryptSymmetric());

        } else {
            Q_FOREACH (const QString &file, files) {
                const std::vector< std::shared_ptr<SignEncryptTask> > created =
                    createSignEncryptTasksForFileInfo(QFileInfo(file), ascii,
                            pgpRecipients.toStdVector(),
                            pgpSigners.toStdVector(),
                            cmsRecipients.toStdVector(),
                            cmsSigners.toStdVector(),
                            buildOutputNamesForDir(file, wizard->outputNames()),
                            wizard->encryptSymmetric());
                tasks.insert(tasks.end(), created.begin(), created.end());
            }
        }

        const std::shared_ptr<OverwritePolicy> overwritePolicy(new OverwritePolicy(wizard));
        Q_FOREACH (const std::shared_ptr<SignEncryptTask> &i, tasks) {
            i->setOverwritePolicy(overwritePolicy);
        }

        kleo_assert(runnable.empty());

        runnable.swap(tasks);

        for (const std::shared_ptr<Task> &task : qAsConst(runnable)) {
            q->connectTask(task);
        }

        std::shared_ptr<TaskCollection> coll(new TaskCollection);

        std::vector<std::shared_ptr<Task> > tmp;
        std::copy(runnable.begin(), runnable.end(), std::back_inserter(tmp));
        coll->setTasks(tmp);
        wizard->setTaskCollection(coll);

        QTimer::singleShot(0, q, SLOT(schedule()));

    } catch (const Kleo::Exception &e) {
        reportError(e.error().encodedError(), e.message());
    } catch (const std::exception &e) {
        reportError(gpg_error(GPG_ERR_UNEXPECTED),
                    i18n("Caught unexpected exception in SignEncryptFilesController::Private::slotWizardOperationPrepared: %1",
                         QString::fromLocal8Bit(e.what())));
    } catch (...) {
        reportError(gpg_error(GPG_ERR_UNEXPECTED),
                    i18n("Caught unknown exception in SignEncryptFilesController::Private::slotWizardOperationPrepared"));
    }
}

void SignEncryptFilesController::Private::schedule()
{

    if (!cms)
        if (const std::shared_ptr<SignEncryptTask> t = takeRunnable(CMS)) {
            t->start();
            cms = t;
        }

    if (!openpgp)
        if (const std::shared_ptr<SignEncryptTask> t = takeRunnable(OpenPGP)) {
            t->start();
            openpgp = t;
        }

    if (!cms && !openpgp) {
        kleo_assert(runnable.empty());
        q->emitDoneOrError();
    }
}

std::shared_ptr<SignEncryptTask> SignEncryptFilesController::Private::takeRunnable(GpgME::Protocol proto)
{
    const auto it = std::find_if(runnable.begin(), runnable.end(),
                                 [proto](const std::shared_ptr<Task> &task) { return task->protocol() == proto; });
    if (it == runnable.end()) {
        return std::shared_ptr<SignEncryptTask>();
    }

    const std::shared_ptr<SignEncryptTask> result = *it;
    runnable.erase(it);
    return result;
}

void SignEncryptFilesController::doTaskDone(const Task *task, const std::shared_ptr<const Task::Result> &result)
{
    Q_UNUSED(result)
    Q_ASSERT(task);

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

void SignEncryptFilesController::cancel()
{
    qCDebug(KLEOPATRA_LOG);
    try {
        if (d->wizard) {
            d->wizard->close();
        }
        d->cancelAllTasks();
    } catch (const std::exception &e) {
        qCDebug(KLEOPATRA_LOG) << "Caught exception: " << e.what();
    }
}

void SignEncryptFilesController::Private::cancelAllTasks()
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

void SignEncryptFilesController::Private::ensureWizardCreated()
{
    if (wizard) {
        return;
    }

    std::unique_ptr<SignEncryptFilesWizard> w(new SignEncryptFilesWizard);
    w->setAttribute(Qt::WA_DeleteOnClose);

    connect(w.get(), SIGNAL(operationPrepared()), q, SLOT(slotWizardOperationPrepared()), Qt::QueuedConnection);
    connect(w.get(), SIGNAL(rejected()), q, SLOT(slotWizardCanceled()), Qt::QueuedConnection);
    wizard = w.release();

    updateWizardMode();
}

void SignEncryptFilesController::Private::ensureWizardVisible()
{
    ensureWizardCreated();
    q->bringToForeground(wizard);
}

#include "moc_signencryptfilescontroller.cpp"
