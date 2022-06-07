/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/newopenpgpcertificateresultdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "newopenpgpcertificateresultdialog.h"

#include "commands/exportcertificatecommand.h"
#include "commands/exportopenpgpcertstoservercommand.h"
#ifdef QGPGME_SUPPORTS_SECRET_KEY_EXPORT
#include "commands/exportsecretkeycommand.h"
#else
#include "commands/exportsecretkeycommand_old.h"
#endif
#include "utils/email.h"
#include "utils/scrollarea.h"

#include <Libkleo/Debug>
#include <Libkleo/Formatting>

#include <KLocalizedString>
#include <KMessageBox>
#include <KSeparator>

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include <gpgme++/key.h>
#include <gpgme++/keygenerationresult.h>

#include <kleopatra_debug.h>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;
#ifndef QGPGME_SUPPORTS_SECRET_KEY_EXPORT
using Kleo::Commands::Compat::ExportSecretKeyCommand;
#endif

class NewOpenPGPCertificateResultDialog::Private
{
    friend class ::Kleo::NewOpenPGPCertificateResultDialog;
    NewOpenPGPCertificateResultDialog *const q;

    struct UI {
        QLabel *infoLabel = nullptr;
        QPushButton *makeBackupPB = nullptr;
        QPushButton *sendCertificateByEMailPB = nullptr;
        QPushButton *uploadToKeyserverPB = nullptr;
        QDialogButtonBox *buttonBox = nullptr;

        UI(QDialog *parent)
        {
            auto mainLayout = new QVBoxLayout{parent};

            infoLabel = new QLabel{parent};
            infoLabel->setWordWrap(true);
            mainLayout->addWidget(infoLabel);

            mainLayout->addWidget(new KSeparator{Qt::Horizontal, parent});

            auto scrollArea = new ScrollArea{parent};
            scrollArea->setFocusPolicy(Qt::NoFocus);
            scrollArea->setFrameStyle(QFrame::NoFrame);
            scrollArea->setBackgroundRole(parent->backgroundRole());
            scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scrollArea->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
            auto scrollAreaLayout = qobject_cast<QBoxLayout *>(scrollArea->widget()->layout());
            scrollAreaLayout->setContentsMargins(0, 0, 0, 0);

            auto nextStepsGB = new QGroupBox{i18nc("@title:group", "Next Steps"), parent};
            nextStepsGB->setFlat(true);
            auto nextStepsGBLayout = new QVBoxLayout{nextStepsGB};

            makeBackupPB = new QPushButton{i18nc("@action:button", "Make a Backup Of Your Key Pair..."), nextStepsGB};
            nextStepsGBLayout->addWidget(makeBackupPB);

            sendCertificateByEMailPB = new QPushButton{i18nc("@action:button", "Send Public Key By EMail..."), nextStepsGB};
            nextStepsGBLayout->addWidget(sendCertificateByEMailPB);

            uploadToKeyserverPB = new QPushButton{i18nc("@action:button", "Upload Public Key To Directory Service..."), nextStepsGB};
            nextStepsGBLayout->addWidget(uploadToKeyserverPB);

            scrollAreaLayout->addWidget(nextStepsGB);

            mainLayout->addWidget(scrollArea);

            mainLayout->addStretch(1);

            mainLayout->addWidget(new KSeparator{Qt::Horizontal, parent});

            buttonBox = new QDialogButtonBox{QDialogButtonBox::Retry | QDialogButtonBox::Close, parent};
            buttonBox->button(QDialogButtonBox::Retry)->setAutoDefault(false);
            buttonBox->button(QDialogButtonBox::Close)->setAutoDefault(false);

            mainLayout->addWidget(buttonBox);
        }
    } ui;

public:
    Private(const KeyGenerationResult &result, const Key &key, NewOpenPGPCertificateResultDialog *qq);

    void slotSendCertificateByEMail();
    void slotSendCertificateByEMailContinuation();
    void slotUploadCertificateToDirectoryServer();
    void slotBackupCertificate();

private:
    KeyGenerationResult result;
    Key key;
    std::unique_ptr<QTemporaryDir> tmpDir;
    QPointer<ExportCertificateCommand> exportCertificateCommand;
};

NewOpenPGPCertificateResultDialog::Private::Private(const KeyGenerationResult &result_, const Key &key_, NewOpenPGPCertificateResultDialog *qq)
    : q{qq}
    , ui{qq}
    , result{result_}
    , key{key_}
{
    if (key.protocol() != GpgME::OpenPGP) {
        qCWarning(KLEOPATRA_LOG) << q << "Key has wrong protocol:" << key;
        key = Key{};
    }

    q->setWindowTitle(i18nc("@title:window", "Success"));

    ui.infoLabel->setText(
        xi18n("<para>A new OpenPGP certificate was created successfully. Find some suggested next steps below.</para>"
              "<para>Fingerprint of the new certificate: %1</para>",
              Formatting::prettyID(key.primaryFingerprint())));

    connect(ui.sendCertificateByEMailPB, &QPushButton::clicked, q, [this]() {
        slotSendCertificateByEMail();
    });
    connect(ui.uploadToKeyserverPB, &QPushButton::clicked, q, [this]() {
        slotUploadCertificateToDirectoryServer();
    });
    connect(ui.makeBackupPB, &QPushButton::clicked, q, [this]() {
        slotBackupCertificate();
    });

    // handle the Retry button
    connect(ui.buttonBox, &QDialogButtonBox::accepted, q, [this]() {
        Q_EMIT q->retry();
        q->done(-1); // neither Accepted nor Rejected
    });
    // handle the Close button
    connect(ui.buttonBox, &QDialogButtonBox::rejected, q, &QDialog::accept);
}

void NewOpenPGPCertificateResultDialog::Private::slotSendCertificateByEMail()
{
    if (key.isNull() || exportCertificateCommand) {
        return;
    }
    auto cmd = new ExportCertificateCommand{key};
    if (!tmpDir) {
        tmpDir = std::make_unique<QTemporaryDir>();
    }
    const QString filename = QString::fromLatin1(key.primaryFingerprint()) + QLatin1String(".asc");
    const QString filePath = QDir{tmpDir->path()}.absoluteFilePath(filename);
    cmd->setOpenPGPFileName(filePath);
    connect(cmd, &ExportCertificateCommand::finished, q, [this]() {
        slotSendCertificateByEMailContinuation();
    });
    cmd->start();
    exportCertificateCommand = cmd;
}

void NewOpenPGPCertificateResultDialog::Private::slotSendCertificateByEMailContinuation()
{
    if (!exportCertificateCommand) {
        return;
    }
    // ### better error handling?
    const QString filePath = exportCertificateCommand->openPGPFileName();
    qCDebug(KLEOPATRA_LOG) << __func__ << "filePath:" << filePath;
    exportCertificateCommand = nullptr;
    if (filePath.isEmpty()) {
        return;
    }
    invokeMailer(i18n("My new public OpenPGP key"), i18n("Please find attached my new public OpenPGP key."), QFileInfo{filePath});
    KMessageBox::information(q,
                             xi18nc("@info",
                                    "<para><application>Kleopatra</application> tried to send a mail via your default mail client.</para>"
                                    "<para>Some mail clients are known not to support attachments when invoked this way.</para>"
                                    "<para>If your mail client does not have an attachment, then attach the file <filename>%1</filename> manually.</para>",
                                    filePath),
                             i18nc("@title:window", "Sending Mail"),
                             QStringLiteral("newcertificatewizard-openpgp-mailto-troubles"));
}

void NewOpenPGPCertificateResultDialog::Private::slotUploadCertificateToDirectoryServer()
{
    if (key.isNull()) {
        return;
    }
    (new ExportOpenPGPCertsToServerCommand{key})->start();
}

void NewOpenPGPCertificateResultDialog::Private::slotBackupCertificate()
{
    if (key.isNull()) {
        return;
    }
    (new ExportSecretKeyCommand{key})->start();
}

NewOpenPGPCertificateResultDialog::NewOpenPGPCertificateResultDialog(const GpgME::KeyGenerationResult &result,
                                                                     const GpgME::Key &key,
                                                                     QWidget *parent,
                                                                     Qt::WindowFlags f)
    : QDialog{parent, f}
    , d{new Private{result, key, this}}
{
    Q_ASSERT(!result.error().code() && result.fingerprint() && !key.isNull() && (key.protocol() == GpgME::OpenPGP)
             && !qstrcmp(result.fingerprint(), key.primaryFingerprint()));
}

NewOpenPGPCertificateResultDialog::~NewOpenPGPCertificateResultDialog() = default;
