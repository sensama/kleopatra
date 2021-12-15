/* -*- mode: c++; c-basic-offset:4 -*-
    commands/importcertificatescommand_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007, 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "command_p.h"
#include "importcertificatescommand.h"

#include <Libkleo/KeyGroup>

#include <gpgme++/global.h>
#include <gpgme++/importresult.h>

#include <vector>
#include <map>

namespace GpgME
{
class Import;
class KeyListResult;
class Error;
}

namespace QGpgME
{
class AbstractImportJob;
}

namespace Kleo
{
class KeyCacheAutoRefreshSuspension;
}

class QByteArray;

enum class ImportType
{
    Local,
    External,
};

struct ImportJobData
{
    QString id;
    GpgME::Protocol protocol;
    ImportType type;
    QGpgME::AbstractImportJob *job;
};

bool operator==(const ImportJobData &lhs, const ImportJobData &rhs);

struct ImportResultData
{
    QString id;
    GpgME::Protocol protocol;
    ImportType type;
    GpgME::ImportResult result;
};

struct ImportedGroup
{
    enum class Status
    {
        New,
        Updated,
    };
    QString id;
    Kleo::KeyGroup group;
    Status status;
};

struct ImportOptions
{
    QString importFilter;
    GpgME::Key::Origin keyOrigin = GpgME::Key::OriginUnknown;
    QString keyOriginUrl;
};

class Kleo::ImportCertificatesCommand::Private : public Command::Private
{
    friend class ::Kleo::ImportCertificatesCommand;
    Kleo::ImportCertificatesCommand *q_func() const
    {
        return static_cast<ImportCertificatesCommand *>(q);
    }

public:
    explicit Private(ImportCertificatesCommand *qq, KeyListController *c);
    ~Private() override;

    void setWaitForMoreJobs(bool waiting);

    void startImport(GpgME::Protocol proto, const QByteArray &data, const QString &id = QString(), const ImportOptions &options = {});
    void startImport(GpgME::Protocol proto, const std::vector<GpgME::Key> &keys, const QString &id = QString());
    void importResult(const GpgME::ImportResult &);
    void importResult(const ImportResultData &result);

    void importGroupsFromFile(const QString &filename);

    void showError(QWidget *parent, const GpgME::Error &error, const QString &id = QString());
    void showError(const GpgME::Error &error, const QString &id = QString());

    void setImportResultProxyModel(const std::vector<ImportResultData> &results);

    bool showPleaseCertify(const GpgME::Import &imp);

    void keyListDone(const GpgME::KeyListResult &result,
                     const std::vector<GpgME::Key> &keys,
                     const QString &, const GpgME::Error&);

private:
    void showDetails(const std::vector<ImportResultData> &results,
                     const std::vector<ImportedGroup> &groups);
    void processResults();
    void tryToFinish();
    void keyCacheUpdated();
    void importGroups();

private:
    bool waitForMoreJobs = false;
    std::vector<GpgME::Protocol> nonWorkingProtocols;
    std::vector<ImportJobData> jobs;
    std::vector<QString> filesToImportGroupsFrom;
    std::vector<ImportResultData> results;
    std::vector<ImportedGroup> importedGroups;
    std::shared_ptr<KeyCacheAutoRefreshSuspension> keyCacheAutoRefreshSuspension;
    QMetaObject::Connection keyListConnection;
};

inline Kleo::ImportCertificatesCommand::Private *Kleo::ImportCertificatesCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
inline const Kleo::ImportCertificatesCommand::Private *Kleo::ImportCertificatesCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

inline Kleo::ImportCertificatesCommand::ImportCertificatesCommand(Private *pp) : Command(pp) {}
inline Kleo::ImportCertificatesCommand::ImportCertificatesCommand(QAbstractItemView *v, Private *pp) : Command(v, pp) {}


