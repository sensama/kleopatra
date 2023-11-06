/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/resultpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "resultpage_p.h"

#include "commands/exportcertificatecommand.h"
#include "commands/exportopenpgpcertstoservercommand.h"
#include "commands/exportsecretkeycommand.h"
#include "utils/dragqueen.h"
#include "utils/email.h"
#include "utils/filedialog.h"
#include "utils/scrollarea.h"

#include <Libkleo/KeyCache>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <gpgme++/key.h>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::NewCertificateUi;
using namespace GpgME;

struct ResultPage::UI {
    QTextBrowser *resultTB = nullptr;
    QTextBrowser *errorTB = nullptr;
    DragQueen *dragQueen = nullptr;
    QPushButton *restartWizardPB = nullptr;
    QGroupBox *nextStepsGB = nullptr;
    QPushButton *saveRequestToFilePB = nullptr;
    QPushButton *sendRequestByEMailPB = nullptr;
    QPushButton *makeBackupPB = nullptr;
    QPushButton *sendCertificateByEMailPB = nullptr;
    QPushButton *uploadToKeyserverPB = nullptr;
    QPushButton *createRevocationRequestPB = nullptr;
    QPushButton *createSigningCertificatePB = nullptr;
    QPushButton *createEncryptionCertificatePB = nullptr;

    UI(QWizardPage *parent)
    {
        auto mainLayout = new QVBoxLayout{parent};
        const auto margins = mainLayout->contentsMargins();
        mainLayout->setContentsMargins(margins.left(), 0, margins.right(), 0);

        auto scrollArea = new ScrollArea{parent};
        scrollArea->setFocusPolicy(Qt::NoFocus);
        scrollArea->setFrameStyle(QFrame::NoFrame);
        scrollArea->setBackgroundRole(parent->backgroundRole());
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
        auto scrollAreaLayout = qobject_cast<QBoxLayout *>(scrollArea->widget()->layout());
        scrollAreaLayout->setContentsMargins(0, margins.top(), 0, margins.bottom());

        auto resultGB = new QGroupBox{i18nc("@title:group", "Result"), parent};
        auto resultGBLayout = new QHBoxLayout{resultGB};

        resultTB = new QTextBrowser{resultGB};
        resultGBLayout->addWidget(resultTB);

        errorTB = new QTextBrowser{resultGB};
        resultGBLayout->addWidget(errorTB);

        dragQueen = new Kleo::DragQueen{resultGB};
        dragQueen->setToolTip(i18n("Drag this icon to your mail application's composer to attach the request to a mail."));
        dragQueen->setAlignment(Qt::AlignCenter);
        resultGBLayout->addWidget(dragQueen);

        scrollAreaLayout->addWidget(resultGB);

        restartWizardPB = new QPushButton{i18n("Restart This Wizard (Keeps Your Parameters)"), parent};

        scrollAreaLayout->addWidget(restartWizardPB);

        nextStepsGB = new QGroupBox{i18nc("@title:group", "Next Steps"), parent};
        auto nextStepsGBLayout = new QVBoxLayout{nextStepsGB};

        saveRequestToFilePB = new QPushButton{i18n("Save Certificate Request To File..."), nextStepsGB};
        nextStepsGBLayout->addWidget(saveRequestToFilePB);

        sendRequestByEMailPB = new QPushButton{i18n("Send Certificate Request By EMail..."), nextStepsGB};
        nextStepsGBLayout->addWidget(sendRequestByEMailPB);

        makeBackupPB = new QPushButton{i18n("Make a Backup Of Your Key Pair..."), nextStepsGB};
        nextStepsGBLayout->addWidget(makeBackupPB);

        sendCertificateByEMailPB = new QPushButton{i18n("Send Public Key By EMail..."), nextStepsGB};
        nextStepsGBLayout->addWidget(sendCertificateByEMailPB);

        uploadToKeyserverPB = new QPushButton{i18n("Upload Public Key To Directory Service..."), nextStepsGB};
        nextStepsGBLayout->addWidget(uploadToKeyserverPB);

        createRevocationRequestPB = new QPushButton{i18n("Create Revocation Request..."), nextStepsGB};
        nextStepsGBLayout->addWidget(createRevocationRequestPB);

        createSigningCertificatePB = new QPushButton{i18n("Create Signing Certificate With Same Parameters"), nextStepsGB};
        nextStepsGBLayout->addWidget(createSigningCertificatePB);

        createEncryptionCertificatePB = new QPushButton{i18n("Create Encryption Certificate With Same Parameters"), nextStepsGB};
        nextStepsGBLayout->addWidget(createEncryptionCertificatePB);

        scrollAreaLayout->addWidget(nextStepsGB);

        mainLayout->addWidget(scrollArea);
    }
};

ResultPage::ResultPage(QWidget *p)
    : WizardPage{p}
    , ui{new UI{this}}
    , initialized{false}
    , successfullyCreatedSigningCertificate{false}
    , successfullyCreatedEncryptionCertificate{false}
{
    setObjectName(QString::fromUtf8("Kleo__NewCertificateUi__ResultPage"));

    connect(ui->saveRequestToFilePB, &QPushButton::clicked, this, &ResultPage::slotSaveRequestToFile);
    connect(ui->sendRequestByEMailPB, &QPushButton::clicked, this, &ResultPage::slotSendRequestByEMail);
    connect(ui->sendCertificateByEMailPB, &QPushButton::clicked, this, &ResultPage::slotSendCertificateByEMail);
    connect(ui->uploadToKeyserverPB, &QPushButton::clicked, this, &ResultPage::slotUploadCertificateToDirectoryServer);
    connect(ui->makeBackupPB, &QPushButton::clicked, this, &ResultPage::slotBackupCertificate);
    connect(ui->createRevocationRequestPB, &QPushButton::clicked, this, &ResultPage::slotCreateRevocationRequest);
    connect(ui->createSigningCertificatePB, &QPushButton::clicked, this, &ResultPage::slotCreateSigningCertificate);
    connect(ui->createEncryptionCertificatePB, &QPushButton::clicked, this, &ResultPage::slotCreateEncryptionCertificate);

    ui->dragQueen->setPixmap(QIcon::fromTheme(QStringLiteral("kleopatra")).pixmap(64, 64));
    registerField(QStringLiteral("error"), ui->errorTB, "plainText");
    registerField(QStringLiteral("result"), ui->resultTB, "plainText");
    registerField(QStringLiteral("url"), ui->dragQueen, "url");
    // hidden field, since QWizard can't deal with non-widget-backed fields...
    auto le = new QLineEdit(this);
    le->hide();
    registerField(QStringLiteral("fingerprint"), le);
}

ResultPage::~ResultPage() = default;

void ResultPage::initializePage()
{
    const bool error = isError();

    if (error) {
        setTitle(i18nc("@title", "Key Creation Failed"));
        setSubTitle(i18n("Key pair creation failed. Please find details about the failure below."));
    } else {
        setTitle(i18nc("@title", "Key Pair Successfully Created"));
        setSubTitle(i18n("Your new key pair was created successfully. Please find details on the result and some suggested next steps below."));
    }

    ui->resultTB->setVisible(!error);
    ui->errorTB->setVisible(error);
    ui->dragQueen->setVisible(!error && !pgp());
    ui->restartWizardPB->setVisible(error);
    ui->nextStepsGB->setVisible(!error);
    ui->saveRequestToFilePB->setVisible(!pgp());
    ui->makeBackupPB->setVisible(pgp());
    ui->createRevocationRequestPB->setVisible(pgp() && false); // not implemented

    ui->sendCertificateByEMailPB->setVisible(pgp());
    ui->sendRequestByEMailPB->setVisible(!pgp());
    ui->uploadToKeyserverPB->setVisible(pgp());

    if (!error && !pgp()) {
        if (signingAllowed() && !encryptionAllowed()) {
            successfullyCreatedSigningCertificate = true;
        } else if (!signingAllowed() && encryptionAllowed()) {
            successfullyCreatedEncryptionCertificate = true;
        } else {
            successfullyCreatedEncryptionCertificate = successfullyCreatedSigningCertificate = true;
        }
    }

    ui->createSigningCertificatePB->setVisible(successfullyCreatedEncryptionCertificate && !successfullyCreatedSigningCertificate);
    ui->createEncryptionCertificatePB->setVisible(successfullyCreatedSigningCertificate && !successfullyCreatedEncryptionCertificate);

    if (error) {
        wizard()->setOptions(wizard()->options() & ~QWizard::NoCancelButtonOnLastPage);
    } else {
        wizard()->setOptions(wizard()->options() | QWizard::NoCancelButtonOnLastPage);
    }

    if (!initialized) {
        connect(ui->restartWizardPB, &QAbstractButton::clicked, this, [this]() {
            restartAtEnterDetailsPage();
        });
    }
    initialized = true;
}

bool ResultPage::isError() const
{
    return !ui->errorTB->document()->isEmpty();
}

bool ResultPage::isComplete() const
{
    return !isError();
}

Key ResultPage::key() const
{
    return KeyCache::instance()->findByFingerprint(fingerprint().toLatin1().constData());
}

void ResultPage::slotSaveRequestToFile()
{
    QString fileName = FileDialog::getSaveFileName(this, i18nc("@title", "Save Request"), QStringLiteral("imp"), i18n("PKCS#10 Requests (*.p10)"));
    if (fileName.isEmpty()) {
        return;
    }
    if (!fileName.endsWith(QLatin1String(".p10"), Qt::CaseInsensitive)) {
        fileName += QLatin1String(".p10");
    }
    QFile src(QUrl(url()).toLocalFile());
    if (!src.copy(fileName))
        KMessageBox::error(this,
                           xi18nc("@info",
                                  "Could not copy temporary file <filename>%1</filename> "
                                  "to file <filename>%2</filename>: <message>%3</message>",
                                  src.fileName(),
                                  fileName,
                                  src.errorString()),
                           i18nc("@title", "Error Saving Request"));
    else
        KMessageBox::information(this,
                                 xi18nc("@info",
                                        "<para>Successfully wrote request to <filename>%1</filename>.</para>"
                                        "<para>You should now send the request to the Certification Authority (CA).</para>",
                                        fileName),
                                 i18nc("@title", "Request Saved"));
}

void ResultPage::slotSendRequestByEMail()
{
    if (pgp()) {
        return;
    }
    const KConfigGroup config(KSharedConfig::openConfig(), QLatin1String("CertificateCreationWizard"));
    invokeMailer(config.readEntry("CAEmailAddress"), // to
                 i18n("Please process this certificate."), // subject
                 i18n("Please process this certificate and inform the sender about the location to fetch the resulting certificate.\n\nThanks,\n"), // body
                 QFileInfo{QUrl(url()).toLocalFile()}); // attachment
    KMessageBox::information(this,
                             xi18nc("@info",
                                    "<para><application>Kleopatra</application> tried to send a mail via your default mail client.</para>"
                                    "<para>Some mail clients are known not to support attachments when invoked this way.</para>"
                                    "<para>If your mail client does not have an attachment, then drag the <application>Kleopatra</application> icon and drop "
                                    "it on the message compose window of your mail client.</para>"
                                    "<para>If that does not work, either, save the request to a file, and then attach that.</para>"),
                             i18nc("@title", "Sending Mail"),
                             QStringLiteral("newcertificatewizard-mailto-troubles"));
}

void ResultPage::slotSendCertificateByEMail()
{
    if (!pgp() || exportCertificateCommand) {
        return;
    }
    auto cmd = new ExportCertificateCommand(key());
    connect(cmd, &ExportCertificateCommand::finished, this, &ResultPage::slotSendCertificateByEMailContinuation);
    cmd->setOpenPGPFileName(tmpDir().absoluteFilePath(fingerprint() + QLatin1String(".asc")));
    cmd->start();
    exportCertificateCommand = cmd;
}

void ResultPage::slotSendCertificateByEMailContinuation()
{
    if (!exportCertificateCommand) {
        return;
    }
    // ### better error handling?
    const QString fileName = exportCertificateCommand->openPGPFileName();
    qCDebug(KLEOPATRA_LOG) << "fileName" << fileName;
    exportCertificateCommand = nullptr;
    if (fileName.isEmpty()) {
        return;
    }
    invokeMailer(i18n("My new public OpenPGP key"), // subject
                 i18n("Please find attached my new public OpenPGP key."), // body
                 QFileInfo{fileName});
    KMessageBox::information(this,
                             xi18nc("@info",
                                    "<para><application>Kleopatra</application> tried to send a mail via your default mail client.</para>"
                                    "<para>Some mail clients are known not to support attachments when invoked this way.</para>"
                                    "<para>If your mail client does not have an attachment, then attach the file <filename>%1</filename> manually.</para>",
                                    fileName),
                             i18nc("@title", "Sending Mail"),
                             QStringLiteral("newcertificatewizard-openpgp-mailto-troubles"));
}

void ResultPage::slotUploadCertificateToDirectoryServer()
{
    if (pgp()) {
        (new ExportOpenPGPCertsToServerCommand(key()))->start();
    }
}

void ResultPage::slotBackupCertificate()
{
    if (pgp()) {
        (new ExportSecretKeyCommand(key()))->start();
    }
}

void ResultPage::slotCreateRevocationRequest()
{
}

void ResultPage::slotCreateSigningCertificate()
{
    if (successfullyCreatedSigningCertificate) {
        return;
    }
    toggleSignEncryptAndRestart();
}

void ResultPage::slotCreateEncryptionCertificate()
{
    if (successfullyCreatedEncryptionCertificate) {
        return;
    }
    toggleSignEncryptAndRestart();
}

void ResultPage::toggleSignEncryptAndRestart()
{
    if (!wizard()) {
        return;
    }
    if (KMessageBox::warningContinueCancel(this,
                                           i18nc("@info",
                                                 "This operation will delete the certification request. "
                                                 "Please make sure that you have sent or saved it before proceeding."),
                                           i18nc("@title", "Certification Request About To Be Deleted"))
        != KMessageBox::Continue) {
        return;
    }
    const bool sign = signingAllowed();
    const bool encr = encryptionAllowed();
    setField(QStringLiteral("signingAllowed"), !sign);
    setField(QStringLiteral("encryptionAllowed"), !encr);
    restartAtEnterDetailsPage();
}

#include "moc_resultpage_p.cpp"
