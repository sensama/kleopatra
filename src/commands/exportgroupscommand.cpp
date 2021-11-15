/* -*- mode: c++; c-basic-offset:4 -*-
    exportgroupscommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "exportgroupscommand.h"
#include "command_p.h"

#include "utils/filedialog.h"

#include <Libkleo/Algorithm>
#include <Libkleo/KeyGroup>
#include <Libkleo/KeyGroupImportExport>
#include <Libkleo/KeyHelpers>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QGpgME/Protocol>
#include <QGpgME/ExportJob>

#include <QFileInfo>
#include <QStandardPaths>

#include <memory>
#include <vector>

using namespace Kleo;
using namespace GpgME;
using namespace QGpgME;

namespace
{

static const QString certificateGroupFileExtension{QLatin1String{".kgrp"}};

QString getLastUsedExportDirectory()
{
    KConfigGroup config{KSharedConfig::openConfig(), "ExportDialog"};
    return config.readEntry("LastDirectory", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
}

void updateLastUsedExportDirectory(const QString &path)
{
    KConfigGroup config{KSharedConfig::openConfig(), "ExportDialog"};
    config.writeEntry("LastDirectory", QFileInfo{path}.absolutePath());
}

QString proposeFilename(const std::vector<KeyGroup> &groups)
{
    QString filename;

    filename = getLastUsedExportDirectory() + QLatin1Char{'/'};
    if (groups.size() == 1) {
        filename += groups.front().name().replace(QLatin1Char{'/'}, QLatin1Char{'_'});
    } else {
        filename += i18nc("A generic filename for exported certificate groups", "certificate groups");
    }

    return filename + certificateGroupFileExtension;
}

QString requestFilename(QWidget *parent, const std::vector<KeyGroup> &groups)
{
    const QString proposedFilename = proposeFilename(groups);

    auto filename = FileDialog::getSaveFileNameEx(
        parent,
        i18ncp("@title:window", "Export Certificate Group", "Export Certificate Groups", groups.size()),
        QStringLiteral("imp"),
        proposedFilename,
        i18nc("filename filter like Certificate Groups (*.foo)", "Certificate Groups (*%1)", certificateGroupFileExtension));
    if (!filename.isEmpty()) {
        const QFileInfo fi{filename};
        if (fi.suffix().isEmpty()) {
            filename += certificateGroupFileExtension;
        }
        updateLastUsedExportDirectory(filename);
    }

    return filename;
}

}

class ExportGroupsCommand::Private : public Command::Private
{
    friend class ::ExportGroupsCommand;
    ExportGroupsCommand *q_func() const
    {
        return static_cast<ExportGroupsCommand *>(q);
    }
public:
    explicit Private(ExportGroupsCommand *qq);
    ~Private() override;

    void start();

    bool exportGroups();
    bool startExportJob(GpgME::Protocol protocol, const std::vector<Key> &keys);

    void onExportJobResult(const QGpgME::Job *job, const GpgME::Error &err, const QByteArray &keyData);

    void cancelJobs();
    void showError(const GpgME::Error &err);

    void finishedIfLastJob(const QGpgME::Job *job);

private:
    std::vector<KeyGroup> groups;
    QString filename;
    std::vector<QPointer<QGpgME::Job>> exportJobs;
};

ExportGroupsCommand::Private *ExportGroupsCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const ExportGroupsCommand::Private *ExportGroupsCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

ExportGroupsCommand::Private::Private(ExportGroupsCommand *qq)
    : Command::Private(qq)
{
}

ExportGroupsCommand::Private::~Private() = default;

void ExportGroupsCommand::Private::start()
{
    if (groups.empty()) {
        finished();
        return;
    }

    filename = requestFilename(parentWidgetOrView(), groups);
    if (filename.isEmpty()) {
        canceled();
        return;
    }

    const auto groupKeys = std::accumulate(std::begin(groups), std::end(groups),
                                           KeyGroup::Keys{},
                                           [](auto &allKeys, const auto &group) {
                                                const auto keys = group.keys();
                                                allKeys.insert(std::begin(keys), std::end(keys));
                                                return allKeys;
                                           });

    std::vector<Key> openpgpKeys;
    std::vector<Key> cmsKeys;
    std::partition_copy(std::begin(groupKeys), std::end(groupKeys),
                        std::back_inserter(openpgpKeys),
                        std::back_inserter(cmsKeys),
                        [](const GpgME::Key &key) {
                            return key.protocol() == GpgME::OpenPGP;
                        });

    // remove/overwrite existing file
    if (QFile::exists(filename) && !QFile::remove(filename)) {
        error(xi18n("Cannot overwrite existing <filename>%1</filename>.", filename),
              i18nc("@title:window", "Export Failed"));
        finished();
        return;
    }
    if (!exportGroups()) {
        finished();
        return;
    }
    if (!openpgpKeys.empty()) {
        if (!startExportJob(GpgME::OpenPGP, openpgpKeys)) {
            finished();
            return;
        }
    }
    if (!cmsKeys.empty()) {
        if (!startExportJob(GpgME::CMS, cmsKeys)) {
            finishedIfLastJob(nullptr);
        }
    }
}

bool ExportGroupsCommand::Private::exportGroups()
{
    const auto result = writeKeyGroups(filename, groups);
    if (result != WriteKeyGroups::Success) {
        error(xi18n("Writing groups to file <filename>%1</filename> failed.", filename),
              i18nc("@title:window", "Export Failed"));
    }
    return result == WriteKeyGroups::Success;
}

bool ExportGroupsCommand::Private::startExportJob(GpgME::Protocol protocol, const std::vector<Key> &keys)
{
    const QGpgME::Protocol *const backend = (protocol == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    Q_ASSERT(backend);
    std::unique_ptr<ExportJob> jobOwner(backend->publicKeyExportJob(/*armor=*/ true));
    auto job = jobOwner.get();
    Q_ASSERT(job);

    connect(job, &ExportJob::result,
            q, [this, job](const GpgME::Error &err, const QByteArray &keyData) {
                onExportJobResult(job, err, keyData);
            });
    connect(job, &Job::progress,
            q, &Command::progress);

    const GpgME::Error err = job->start(Kleo::getFingerprints(keys));
    if (err) {
        showError(err);
        return false;
    }
    Q_EMIT q->info(i18n("Exporting certificate groups..."));

    exportJobs.push_back(jobOwner.release());
    return true;
}

void ExportGroupsCommand::Private::onExportJobResult(const QGpgME::Job *job, const GpgME::Error &err, const QByteArray &keyData)
{
    Q_ASSERT(Kleo::contains(exportJobs, job));

    if (err) {
        showError(err);
        finishedIfLastJob(job);
        return;
    }

    QFile f{filename};
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append)) {
        error(xi18n("Cannot open file <filename>%1</filename> for writing.", filename),
              i18nc("@title:window", "Export Failed"));
        finishedIfLastJob(job);
        return;
    }

    const auto bytesWritten = f.write(keyData);
    if (bytesWritten != keyData.size()) {
        error(xi18n("Writing certificates to file <filename>%1</filename> failed.", filename),
              i18nc("@title:window", "Export Failed"));
    }

    finishedIfLastJob(job);
}

void ExportGroupsCommand::Private::showError(const GpgME::Error &err)
{
    error(xi18n("<para>An error occurred during the export:</para>"
                "<para><message>%1</message></para>",
                QString::fromLocal8Bit(err.asString())),
          i18nc("@title:window", "Export Failed"));
}

void ExportGroupsCommand::Private::finishedIfLastJob(const QGpgME::Job *job)
{
    if (job) {
        exportJobs.erase(std::remove(exportJobs.begin(), exportJobs.end(), job), exportJobs.end());
    }
    if (exportJobs.size() == 0) {
        finished();
    }
}

void ExportGroupsCommand::Private::cancelJobs()
{
    std::for_each(std::cbegin(exportJobs), std::cend(exportJobs),
                  [](const auto &job) {
                      if (job) {
                          job->slotCancel();
                      }
                  });
    exportJobs.clear();
}

ExportGroupsCommand::ExportGroupsCommand(const std::vector<KeyGroup> &groups)
    : Command{new Private{this}}
{
    d->groups = groups;
}

ExportGroupsCommand::~ExportGroupsCommand() = default;

void ExportGroupsCommand::doStart()
{
    d->start();
}

void ExportGroupsCommand::doCancel()
{
    d->cancelJobs();
}

#undef d
#undef q

#include "moc_exportgroupscommand.cpp"
