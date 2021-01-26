/* -*- mode: c++; c-basic-offset:4 -*-
    exportcertificatecommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2021 g10 Code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "exportcertificatecommand.h"
#include "fileoperationspreferences.h"

#include "command_p.h"

#include <utils/filedialog.h>

#include <Libkleo/Classify>
#include <Libkleo/Formatting>

#include <QGpgME/Protocol>
#include <QGpgME/ExportJob>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include <QSaveFile>

#include <QMap>
#include <QPointer>
#include <QRegExp>
#include <QFileInfo>

#include <algorithm>
#include <vector>

using namespace Kleo;
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
    uint jobsPending = 0;
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
    : Command::Private(qq, c)
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
        if (!fileNames[GpgME::OpenPGP].isEmpty() && !fileNames[GpgME::CMS].isEmpty()) {
            return true;
        }

        /* Unkown protocol ask for first PGP Export file name */
        if (fileNames[GpgME::OpenPGP].isEmpty() && !requestFileNames(GpgME::OpenPGP)) {
            return false;
        }
        /* And then for CMS */
        return requestFileNames(GpgME::CMS);
    }

    if (!fileNames[protocol].isEmpty()) {
        return true;
    }

    KConfigGroup config(KSharedConfig::openConfig(), "ExportDialog");
    const auto lastDir = config.readEntry("LastDirectory", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));

    QString proposedFileName = lastDir + QLatin1Char('/');
    if (keys().size() == 1) {
        const bool usePGPFileExt = FileOperationsPreferences().usePGPFileExt();
        const auto key = keys().front();
        auto name = Formatting::prettyName(key);
        if (name.isEmpty()) {
            name = Formatting::prettyEMail(key);
        }
        /* Not translated so it's better to use in tutorials etc. */
        proposedFileName += QStringLiteral("%1_%2_public.%3").arg(name).arg(
                Formatting::prettyKeyID(key.shortKeyID())).arg(
                QString::fromLatin1(outputFileExtension(protocol == OpenPGP
                        ? Class::OpenPGP | Class::Ascii | Class::Certificate
                        : Class::CMS | Class::Ascii | Class::Certificate, usePGPFileExt)));
    }
    if (protocol == GpgME::CMS) {
        if (!fileNames[GpgME::OpenPGP].isEmpty()) {
            /* If the user has already selected a PGP file name then use that as basis
             * for a proposal for the S/MIME file. */
            proposedFileName = fileNames[GpgME::OpenPGP];
            proposedFileName.replace(QRegExp(QStringLiteral(".asc$")), QStringLiteral(".pem"));
            proposedFileName.replace(QRegExp(QStringLiteral(".gpg$")), QStringLiteral(".der"));
            proposedFileName.replace(QRegExp(QStringLiteral(".pgp$")), QStringLiteral(".der"));
        }
    }

    if (proposedFileName.isEmpty()) {
        proposedFileName = lastDir;
        proposedFileName += i18nc("A generic filename for exported certificates", "certificates");
        proposedFileName += protocol == GpgME::OpenPGP ? QStringLiteral(".asc") : QStringLiteral(".pem");
    }

    auto fname = FileDialog::getSaveFileNameEx(parentWidgetOrView(),
                          i18nc("1 is protocol", "Export %1 Certificates", Formatting::displayName(protocol)),
                          QStringLiteral("imp"),
                          proposedFileName,
                          protocol == GpgME::OpenPGP
                          ? i18n("OpenPGP Certificates") + QLatin1String(" (*.asc *.gpg *.pgp)")
                          : i18n("S/MIME Certificates")  + QLatin1String(" (*.pem *.der)"));

    if (!fname.isEmpty() && protocol == GpgME::CMS && fileNames[GpgME::OpenPGP] == fname) {
        KMessageBox::error(parentWidgetOrView(),
                         i18n("You have to select different filenames for different protocols."),
                         i18n("Export Error"));
        return false;
    }
    const QFileInfo fi(fname);
    if (fi.suffix().isEmpty()) {
        fname += protocol == GpgME::OpenPGP ? QStringLiteral(".asc") : QStringLiteral(".pem");
    }

    fileNames[protocol] = fname;
    config.writeEntry("LastDirectory", fi.absolutePath());
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
