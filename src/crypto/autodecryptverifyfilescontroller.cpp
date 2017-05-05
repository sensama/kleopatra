/* -*- mode: c++; c-basic-offset:4 -*-
    autodecryptverifyfilescontroller.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB
                  2016 by Bundesamt für Sicherheit in der Informationstechnik
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

#include "autodecryptverifyfilescontroller.h"

#include <crypto/gui/decryptverifyoperationwidget.h>
#include <crypto/gui/decryptverifyfilesdialog.h>
#include <crypto/decryptverifytask.h>
#include <crypto/taskcollection.h>

#include "commands/decryptverifyfilescommand.h"

#include <utils/gnupg-helper.h>
#include <utils/path-helper.h>
#include <utils/input.h>
#include <utils/output.h>
#include <utils/kleo_assert.h>
#include <utils/archivedefinition.h>

#include <Libkleo/Classify>

#include <KLocalizedString>
#include <KMessageBox>
#include "kleopatra_debug.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>
#include <QFileDialog>
#include <QTemporaryDir>

#include <memory>
#include <vector>

using namespace GpgME;
using namespace Kleo;
using namespace Kleo::Crypto;
using namespace Kleo::Crypto::Gui;

class AutoDecryptVerifyFilesController::Private
{
    AutoDecryptVerifyFilesController *const q;
public:
    explicit Private(AutoDecryptVerifyFilesController *qq);
    ~Private() { qCDebug(KLEOPATRA_LOG); }

    void slotDialogCanceled();
    void schedule();

    void exec();
    std::vector<std::shared_ptr<Task> > buildTasks(const QStringList &, QStringList &);

    void reportError(int err, const QString &details)
    {
        q->setLastError(err, details);
        q->emitDoneOrError();
    }
    void cancelAllTasks();

    QStringList m_passedFiles, m_filesAfterPreparation;
    std::vector<std::shared_ptr<const DecryptVerifyResult> > m_results;
    std::vector<std::shared_ptr<Task> > m_runnableTasks, m_completedTasks;
    std::shared_ptr<Task> m_runningTask;
    bool m_errorDetected;
    DecryptVerifyOperation m_operation;
    DecryptVerifyFilesDialog *m_dialog;
    QTemporaryDir m_workDir;
};

AutoDecryptVerifyFilesController::Private::Private(AutoDecryptVerifyFilesController *qq) : q(qq),
    m_errorDetected(false),
    m_operation(DecryptVerify),
    m_dialog(nullptr)
{
    qRegisterMetaType<VerificationResult>();
}

void AutoDecryptVerifyFilesController::Private::slotDialogCanceled()
{
    qCDebug(KLEOPATRA_LOG);
}

void AutoDecryptVerifyFilesController::Private::schedule()
{
    if (!m_runningTask && !m_runnableTasks.empty()) {
        const std::shared_ptr<Task> t = m_runnableTasks.back();
        m_runnableTasks.pop_back();
        t->start();
        m_runningTask = t;
    }
    if (!m_runningTask) {
        kleo_assert(m_runnableTasks.empty());
        for (const std::shared_ptr<const DecryptVerifyResult> &i : qAsConst(m_results)) {
            Q_EMIT q->verificationResult(i->verificationResult());
        }
    }
}

void AutoDecryptVerifyFilesController::Private::exec()
{
    Q_ASSERT(!m_dialog);

    QStringList undetected;
    std::vector<std::shared_ptr<Task> > tasks = buildTasks(m_passedFiles, undetected);

    if (!undetected.isEmpty()) {
        // Since GpgME 1.7.0 Classification is supposed to be reliable
        // so we really can't do anything with this data.
        reportError(makeGnuPGError(GPG_ERR_GENERAL),
                    xi18n("Failed to find encrypted or signed data in one or more files.<nl/>"
                          "You can manually select what to do with the files now.<nl/>"
                          "If they contain signed or encrypted data please report a bug (see Help->Report Bug)."));
        auto cmd = new Commands::DecryptVerifyFilesCommand(undetected, nullptr, true);
        cmd->start();
    }
    if (tasks.empty()) {
        q->emitDoneOrError();
        return;
    }
    Q_ASSERT(m_runnableTasks.empty());
    m_runnableTasks.swap(tasks);

    std::shared_ptr<TaskCollection> coll(new TaskCollection);
    Q_FOREACH (const std::shared_ptr<Task> &i, m_runnableTasks) {
        q->connectTask(i);
    }
    coll->setTasks(m_runnableTasks);
    m_dialog = new DecryptVerifyFilesDialog(coll);
    m_dialog->setOutputLocation(heuristicBaseDirectory(m_passedFiles));

    QTimer::singleShot(0, q, SLOT(schedule()));
    if (m_dialog->exec() == QDialog::Accepted) {
        const QDir workdir(m_workDir.path());
        const QDir outDir(m_dialog->outputLocation());
        bool overWriteAll = false;
        qCDebug(KLEOPATRA_LOG) << workdir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        Q_FOREACH (const QFileInfo &fi, workdir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
            const auto inpath = fi.absoluteFilePath();
            const auto outpath = outDir.absoluteFilePath(fi.fileName());
            qCDebug(KLEOPATRA_LOG) << "Moving " << inpath << " to " << outpath;
            const QFileInfo ofi(outpath);
            if (ofi.exists()) {
                int sel = KMessageBox::No;
                if (!overWriteAll) {
                    sel = KMessageBox::questionYesNoCancel(m_dialog, i18n("The file <b>%1</b> already exists.\n"
                                                           "Overwrite?", outpath),
                                                           i18n("Overwrite Existing File?"),
                                                           KStandardGuiItem::overwrite(),
                                                           KGuiItem(i18n("Overwrite All")),
                                                           KStandardGuiItem::cancel());
                }
                if (sel == KMessageBox::Cancel) {
                    qCDebug(KLEOPATRA_LOG) << "Overwriting canceled for: " << outpath;
                    continue;
                }
                if (sel == KMessageBox::No) { //Overwrite All
                    overWriteAll = true;
                }
                if (!QFile::remove(outpath)) {
                    reportError(makeGnuPGError(GPG_ERR_GENERAL),
                                xi18n("Failed to delete <filename>%1</filename>.",
                                      outpath));
                    continue;
                }
            }
            if (!QFile::rename(inpath, outpath)) {
                reportError(makeGnuPGError(GPG_ERR_GENERAL),
                            xi18n("Failed to move <filename>%1</filename> to <filename>%2</filename>.",
                                  inpath, outpath));
            }
        }
    }
    q->emitDoneOrError();
    delete m_dialog;
    m_dialog = nullptr;
    return;
}

std::vector< std::shared_ptr<Task> > AutoDecryptVerifyFilesController::Private::buildTasks(const QStringList &fileNames, QStringList &undetected)
{
    std::vector<std::shared_ptr<Task> > tasks;
    for (const QString &fName : fileNames) {
        const auto classification = classify(fName);
        const auto proto = findProtocol(classification);

        QFileInfo fi(fName);
        qCDebug(KLEOPATRA_LOG) << "classified" << fName << "as" << printableClassification(classification);

        if (!fi.isReadable()) {
            reportError(makeGnuPGError(GPG_ERR_ASS_NO_INPUT),
                        xi18n("Cannot open <filename>%1</filename> for reading.", fName));
        } else if (mayBeAnyCertStoreType(classification)) {
            // Trying to verify a certificate. Possible because extensions are often similar
            // for PGP Keys.
            reportError(makeGnuPGError(GPG_ERR_ASS_NO_INPUT),
                        xi18n("The file <filename>%1</filename> contains certificates and can't be decrypted or verified.", fName));
            qCDebug(KLEOPATRA_LOG) << "reported error";
        } else if (isDetachedSignature(classification)) {
            // Detached signature, try to find data or ask the user.
            QString signedDataFileName = findSignedData(fName);
            if (signedDataFileName.isEmpty()) {
                signedDataFileName = QFileDialog::getOpenFileName(nullptr, xi18n("Select the file to verify with \"%1\"", fi.fileName()),
                                                                  fi.dir().dirName());
            }
            if (signedDataFileName.isEmpty()) {
                qCDebug(KLEOPATRA_LOG) << "No signed data selected. Verify abortet.";
                continue;
            }
            qCDebug(KLEOPATRA_LOG) << "Detached verify: " << fName << " Data: " << signedDataFileName;
            std::shared_ptr<VerifyDetachedTask> t(new VerifyDetachedTask);
            t->setInput(Input::createFromFile(fName));
            t->setSignedData(Input::createFromFile(signedDataFileName));
            t->setProtocol(proto);
            tasks.push_back(t);
        } else if (!mayBeAnyMessageType(classification)) {
            // Not a Message? Maybe there is a signature for this file?
            const auto signatures = findSignatures(fName);
            if (!signatures.empty()) {
                for (const QString &sig : signatures) {
                    qCDebug(KLEOPATRA_LOG) << "Guessing: " << sig << " is a signature for: " << fName;
                    std::shared_ptr<VerifyDetachedTask> t(new VerifyDetachedTask);
                    t->setInput(Input::createFromFile(sig));
                    t->setSignedData(Input::createFromFile(fName));
                    t->setProtocol(proto);
                    tasks.push_back(t);
                }
            } else {
                undetected << fName;
                qCDebug(KLEOPATRA_LOG) << "Failed detection for: " << fName << " adding to undetected.";
            }
        } else {
            // Any Message type so we have input and output.
            const auto input = Input::createFromFile(fName);
            const auto archiveDefinitions = ArchiveDefinition::getArchiveDefinitions();

            const auto ad = q->pick_archive_definition(proto, archiveDefinitions, fName);

            const auto wd = QDir(m_workDir.path());

            const auto output =
                ad       ? ad->createOutputFromUnpackCommand(proto, fName, wd) :
                /*else*/   Output::createFromFile(wd.absoluteFilePath(outputFileName(fi.fileName())), false);

            if (isOpaqueSignature(classification)) {
                qCDebug(KLEOPATRA_LOG) << "creating a VerifyOpaqueTask";
                std::shared_ptr<VerifyOpaqueTask> t(new VerifyOpaqueTask);
                t->setInput(input);
                t->setOutput(output);
                t->setProtocol(proto);
                tasks.push_back(t);
            } else {
                // Any message. That is not an opaque signature needs to be
                // decrypted. Verify we always do because we can't know if
                // an encrypted message is also signed.
                qCDebug(KLEOPATRA_LOG) << "creating a DecryptVerifyTask";
                std::shared_ptr<DecryptVerifyTask> t(new DecryptVerifyTask);
                t->setInput(input);
                t->setOutput(output);
                t->setProtocol(proto);
                tasks.push_back(t);
            }
        }
    }

    return tasks;
}

void AutoDecryptVerifyFilesController::setFiles(const QStringList &files)
{
    d->m_passedFiles = files;
}

AutoDecryptVerifyFilesController::AutoDecryptVerifyFilesController(QObject *parent) :
    DecryptVerifyFilesController(parent), d(new Private(this))
{
}

AutoDecryptVerifyFilesController::AutoDecryptVerifyFilesController(const std::shared_ptr<const ExecutionContext> &ctx, QObject *parent) :
    DecryptVerifyFilesController(ctx, parent), d(new Private(this))
{
}

AutoDecryptVerifyFilesController::~AutoDecryptVerifyFilesController()
{
    qCDebug(KLEOPATRA_LOG);
}

void AutoDecryptVerifyFilesController::start()
{
    d->exec();
}

void AutoDecryptVerifyFilesController::setOperation(DecryptVerifyOperation op)
{
    d->m_operation = op;
}

DecryptVerifyOperation AutoDecryptVerifyFilesController::operation() const
{
    return d->m_operation;
}

void AutoDecryptVerifyFilesController::Private::cancelAllTasks()
{

    // we just kill all runnable tasks - this will not result in
    // signal emissions.
    m_runnableTasks.clear();

    // a cancel() will result in a call to
    if (m_runningTask) {
        m_runningTask->cancel();
    }
}

void AutoDecryptVerifyFilesController::cancel()
{
    qCDebug(KLEOPATRA_LOG);
    try {
        d->m_errorDetected = true;
        if (d->m_dialog) {
            d->m_dialog->close();
        }
        d->cancelAllTasks();
    } catch (const std::exception &e) {
        qCDebug(KLEOPATRA_LOG) << "Caught exception: " << e.what();
    }
}

void AutoDecryptVerifyFilesController::doTaskDone(const Task *task, const std::shared_ptr<const Task::Result> &result)
{
    assert(task);
    Q_UNUSED(task);

    // We could just delete the tasks here, but we can't use
    // Qt::QueuedConnection here (we need sender()) and other slots
    // might not yet have executed. Therefore, we push completed tasks
    // into a burial container

    d->m_completedTasks.push_back(d->m_runningTask);
    d->m_runningTask.reset();

    if (const std::shared_ptr<const DecryptVerifyResult> &dvr = std::dynamic_pointer_cast<const DecryptVerifyResult>(result)) {
        d->m_results.push_back(dvr);
    }

    QTimer::singleShot(0, this, SLOT(schedule()));
}
#include "moc_autodecryptverifyfilescontroller.cpp"
