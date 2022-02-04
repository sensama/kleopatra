/*  commands/importcertificatefromkeyservercommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "importcertificatefromkeyservercommand.h"
#include "importcertificatescommand_p.h"

#include <KLocalizedString>

#include <QProgressDialog>

#include "kleopatra_debug.h"

using namespace Kleo;

class ImportCertificateFromKeyserverCommand::Private : public ImportCertificatesCommand::Private
{
    friend class ::ImportCertificateFromKeyserverCommand;
    ImportCertificateFromKeyserverCommand *q_func() const
    {
        return static_cast<ImportCertificateFromKeyserverCommand *>(q);
    }
public:
    explicit Private(ImportCertificateFromKeyserverCommand *qq,
                     const QStringList &keyIds, const QString &id);
    ~Private() override;

private:
    void start();

private:
    QStringList mKeyIds;
    QString mId;
};

ImportCertificateFromKeyserverCommand::Private *ImportCertificateFromKeyserverCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const ImportCertificateFromKeyserverCommand::Private *ImportCertificateFromKeyserverCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define q q_func()
#define d d_func()

ImportCertificateFromKeyserverCommand::Private::Private(ImportCertificateFromKeyserverCommand *qq,
                                                        const QStringList &keyIds, const QString &id)
    : ImportCertificatesCommand::Private{qq, nullptr}
    , mKeyIds{keyIds}
    , mId{id}
{
}

ImportCertificateFromKeyserverCommand::Private::~Private() = default;

void ImportCertificateFromKeyserverCommand::Private::start()
{
    const auto progressDialog = new QProgressDialog{parentWidgetOrView()};
    progressDialog->setAttribute(Qt::WA_DeleteOnClose);
    progressDialog->setModal(true);
    progressDialog->setRange(0, 0);
    progressDialog->setWindowTitle(i18nc("@title:window", "Fetching Keys"));
    progressDialog->setLabelText(i18np("Fetching 1 key... (this can take a while)", "Fetching %1 keys... (this can take a while)", mKeyIds.size()));
    connect(progressDialog, &QProgressDialog::canceled, q, &Command::cancel);
    connect(q, &Command::finished, progressDialog, [progressDialog]() { progressDialog->accept(); });

    setWaitForMoreJobs(true);
    // start one import per key id to allow canceling the key retrieval without
    // losing already retrieved keys
    for (const auto &keyId : mKeyIds) {
        startImport(GpgME::OpenPGP, {keyId}, mId);
    }
    setWaitForMoreJobs(false);

    progressDialog->show();
}

ImportCertificateFromKeyserverCommand::ImportCertificateFromKeyserverCommand(const QStringList &keyIds, const QString &id)
    : ImportCertificatesCommand{new Private{this, keyIds, id}}
{
}

ImportCertificateFromKeyserverCommand::~ImportCertificateFromKeyserverCommand() = default;

void ImportCertificateFromKeyserverCommand::doStart()
{
    d->start();
}

#undef q_func
#undef d_func
