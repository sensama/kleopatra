/* -*- mode: c++; c-basic-offset:4 -*-
    commands/importcertificatescommand_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007, 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "command_p.h"
#include "importcertificatescommand.h"

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

class QByteArray;

struct ImportJobData
{
    QString id;
    QGpgME::AbstractImportJob *job;
};

bool operator==(const ImportJobData &lhs, const ImportJobData &rhs);

struct ImportResultData
{
    QString id;
    GpgME::ImportResult result;
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

    void startImport(GpgME::Protocol proto, const QByteArray &data, const QString &id = QString());
    void startImport(GpgME::Protocol proto, const std::vector<GpgME::Key> &keys, const QString &id = QString());
    void importResult(const GpgME::ImportResult &);
    void importResult(const ImportResultData &result);

    void showError(QWidget *parent, const GpgME::Error &error, const QString &id = QString());
    void showError(const GpgME::Error &error, const QString &id = QString());

    void showDetails(QWidget *parent, const std::vector<ImportResultData> &results);
    void showDetails(const std::vector<ImportResultData> &results);

    void setImportResultProxyModel(const std::vector<ImportResultData> &results);

    bool showPleaseCertify(const GpgME::Import &imp);

    void keyListDone(const GpgME::KeyListResult &result,
                     const std::vector<GpgME::Key> &keys,
                     const QString &, const GpgME::Error&);
private:
    void processResults();
    void tryToFinish();

private:
    bool waitForMoreJobs = false;
    std::vector<GpgME::Protocol> nonWorkingProtocols;
    std::vector<ImportJobData> jobs;
    std::vector<ImportResultData> results;
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


