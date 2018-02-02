/* -*- mode: c++; c-basic-offset:4 -*-
    exportcertificatecommand.cpp

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

#include "exportcertificatecommand.h"
#include "fileoperationspreferences.h"

#include "command_p.h"

#include <dialogs/exportcertificatesdialog.h>

#include <utils/filedialog.h>

#include <Libkleo/Classify>

#include <QGpgME/Protocol>
#include <QGpgME/ExportJob>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include <QSaveFile>

#include <QMap>
#include <QPointer>

#include <algorithm>
#include <vector>

using namespace Kleo;
using namespace Kleo::Dialogs;
using namespace GpgME;
using namespace QGpgME;

class ExportCertificateCommand::Private : public Command::Private
{
    friend class ::ExportCertificateCommand;
    ExportCertificateCommand *q_func() const
    {
        return static_cast<ExportCertificateCommand *>(q);
    }
public:
    explicit Private(ExportCertificateCommand *qq, KeyListController *c);
    ~Private();
    void startExportJob(GpgME::Protocol protocol, const std::vector<Key> &keys);
    void cancelJobs();
    void exportResult(const GpgME::Error &, const QByteArray &);
    void showError(const GpgME::Error &error);

    bool requestFileNames(GpgME::Protocol prot);
    void finishedIfLastJob();

private:
    QMap<GpgME::Protocol, QString> fileNames;
    uint jobsPending;
    QMap<QObject *, QString> outFileForSender;
    QPointer<ExportJob> cmsJob;
    QPointer<ExportJob> pgpJob;
};

ExportCertificateCommand::Private *ExportCertificateCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const ExportCertificateCommand::Private *ExportCertificateCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

ExportCertificateCommand::Private::Private(ExportCertificateCommand *qq, KeyListController *c)
    : Command::Private(qq, c),
      jobsPending(0)
{

}

ExportCertificateCommand::Private::~Private() {}

ExportCertificateCommand::ExportCertificateCommand(KeyListController *p)
    : Command(new Private(this, p))
{

}

ExportCertificateCommand::ExportCertificateCommand(QAbstractItemView *v, KeyListController *p)
    : Command(v, new Private(this, p))
{

}

ExportCertificateCommand::ExportCertificateCommand(const Key &key)
    : Command(key, new Private(this, nullptr))
{

}

ExportCertificateCommand::~ExportCertificateCommand() {}

void ExportCertificateCommand::setOpenPGPFileName(const QString &fileName)
{
    if (!d->jobsPending) {
        d->fileNames[OpenPGP] = fileName;
    }
}

QString ExportCertificateCommand::openPGPFileName() const
{
    return d->fileNames[OpenPGP];
}

void ExportCertificateCommand::setX509FileName(const QString &fileName)
{
    if (!d->jobsPending) {
        d->fileNames[CMS] = fileName;
    }
}

QString ExportCertificateCommand::x509FileName() const
{
    return d->fileNames[CMS];
}

void ExportCertificateCommand::doStart()
{
    std::vector<Key> keys = d->keys();
    if (keys.empty()) {
        return;
    }

    const auto firstCms = std::partition(keys.begin(), keys.end(),
                                         [](const GpgME::Key &key) {
                                            return key.protocol() != GpgME::CMS;
                                         });
    std::vector<Key> openpgp, cms;
    std::copy(keys.begin(), firstCms, std::back_inserter(openpgp));
    std::copy(firstCms, keys.end(), std::back_inserter(cms));
    Q_ASSERT(!openpgp.empty() || !cms.empty());
    const bool haveBoth = !cms.empty() && !openpgp.empty();
    const GpgME::Protocol prot = haveBoth ? UnknownProtocol : (!cms.empty() ? CMS : OpenPGP);
    if (!d->requestFileNames(prot)) {
        Q_EMIT canceled();
        d->finished();
    } else {
        if (!openpgp.empty()) {
            d->startExportJob(GpgME::OpenPGP, openpgp);
        }
        if (!cms.empty()) {
            d->startExportJob(GpgME::CMS, cms);
        }
    }
}

bool ExportCertificateCommand::Private::requestFileNames(GpgME::Protocol protocol)
{
    if (protocol == UnknownProtocol) {
        if (!fileNames[OpenPGP].isEmpty() && !fileNames[CMS].isEmpty()) {
            return true;
        }
        const QPointer<ExportCertificatesDialog> dlg(new ExportCertificatesDialog);
        applyWindowID(dlg);
        dlg->setOpenPgpExportFileName(fileNames[OpenPGP]);
        dlg->setCmsExportFileName(fileNames[CMS]);
        const bool accepted = dlg->exec() == QDialog::Accepted && dlg;
        if (accepted) {
            fileNames[OpenPGP] = dlg->openPgpExportFileName();
            fileNames[CMS] = dlg->cmsExportFileName();
        } else {
            fileNames.clear();
        }
        delete dlg;
        return accepted;
    }

    if (!fileNames[protocol].isEmpty()) {
        return true;
    }

    QString proposedFileName;
    if (keys().size() == 1) {
        const bool usePGPFileExt = FileOperationsPreferences().usePGPFileExt();
        proposedFileName
            = QString::fromLatin1(keys().front().primaryFingerprint())
              + QLatin1Char('.')
              + QString::fromLatin1(outputFileExtension(protocol == OpenPGP
                                    ? Class::OpenPGP | Class::Ascii | Class::Certificate
                                    : Class::CMS | Class::Ascii | Class::Certificate, usePGPFileExt))
              ;
    }

    const QString fname = FileDialog::getSaveFileNameEx(parentWidgetOrView(),
                          i18n("Export Certificates"),
                          QStringLiteral("imp"),
                          proposedFileName,
                          protocol == GpgME::OpenPGP
                          ? i18n("OpenPGP Certificates") + QLatin1String(" (*.asc *.gpg *.pgp)")
                          : i18n("S/MIME Certificates")  + QLatin1String(" (*.pem *.der)"));
    fileNames[protocol] = fname;
    return !fname.isEmpty();
}

void ExportCertificateCommand::Private::startExportJob(GpgME::Protocol protocol, const std::vector<Key> &keys)
{
    Q_ASSERT(protocol != GpgME::UnknownProtocol);

    const QGpgME::Protocol *const backend = (protocol == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    Q_ASSERT(backend);
    const QString fileName = fileNames[protocol];
    const bool binary = protocol == GpgME::OpenPGP
                        ? fileName.endsWith(QLatin1String(".gpg"), Qt::CaseInsensitive) || fileName.endsWith(QLatin1String(".pgp"), Qt::CaseInsensitive)
                        : fileName.endsWith(QLatin1String(".der"), Qt::CaseInsensitive);
    std::unique_ptr<ExportJob> job(backend->publicKeyExportJob(!binary));
    Q_ASSERT(job.get());

    connect(job.get(), SIGNAL(result(GpgME::Error,QByteArray)),
            q, SLOT(exportResult(GpgME::Error,QByteArray)));

    connect(job.get(), &Job::progress,
            q, &Command::progress);

    QStringList fingerprints;
    fingerprints.reserve(keys.size());
    for (const Key &i : keys) {
        fingerprints << QLatin1String(i.primaryFingerprint());
    }

    const GpgME::Error err = job->start(fingerprints);
    if (err) {
        showError(err);
        finished();
        return;
    }
    Q_EMIT q->info(i18n("Exporting certificates..."));
    ++jobsPending;
    const QPointer<ExportJob> exportJob(job.release());

    outFileForSender[exportJob.data()] = fileName;
    (protocol == CMS ? cmsJob : pgpJob) = exportJob;
}

void ExportCertificateCommand::Private::showError(const GpgME::Error &err)
{
    Q_ASSERT(err);
    const QString msg = i18n("<qt><p>An error occurred while trying to export "
                             "the certificate:</p>"
                             "<p><b>%1</b></p></qt>",
                             QString::fromLocal8Bit(err.asString()));
    error(msg, i18n("Certificate Export Failed"));
}

void ExportCertificateCommand::doCancel()
{
    d->cancelJobs();
}

void ExportCertificateCommand::Private::finishedIfLastJob()
{
    if (jobsPending <= 0) {
        finished();
    }
}

static bool write_complete(QIODevice &iod, const QByteArray &data)
{
    qint64 total = 0;
    qint64 toWrite = data.size();
    while (total < toWrite) {
        const qint64 written = iod.write(data.data() + total, toWrite);
        if (written < 0) {
            return false;
        }
        total += written;
        toWrite -= written;
    }
    return true;
}

void ExportCertificateCommand::Private::exportResult(const GpgME::Error &err, const QByteArray &data)
{
    Q_ASSERT(jobsPending > 0);
    --jobsPending;

    Q_ASSERT(outFileForSender.contains(q->sender()));
    const QString outFile = outFileForSender[q->sender()];

    if (err) {
        showError(err);
        finishedIfLastJob();
        return;
    }
    QSaveFile savefile(outFile);
    //TODO: use KIO
    const QString writeErrorMsg = i18n("Could not write to file %1.",  outFile);
    const QString errorCaption = i18n("Certificate Export Failed");
    if (!savefile.open(QIODevice::WriteOnly)) {
        error(writeErrorMsg, errorCaption);
        finishedIfLastJob();
        return;
    }

    if (!write_complete(savefile, data) ||
            !savefile.commit()) {
        error(writeErrorMsg, errorCaption);
    }
    finishedIfLastJob();
}

void ExportCertificateCommand::Private::cancelJobs()
{
    if (cmsJob) {
        cmsJob->slotCancel();
    }
    if (pgpJob) {
        pgpJob->slotCancel();
    }
}

#undef d
#undef q

#include "moc_exportcertificatecommand.cpp"
