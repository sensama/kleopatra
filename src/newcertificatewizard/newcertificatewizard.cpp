/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/newcertificatewizard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "newcertificatewizard.h"

#include <settings.h>

#include "chooseprotocolpage_p.h"
#include "enterdetailspage_p.h"
#include "keyalgo_p.h"
#include "keycreationpage_p.h"
#include "ui_resultpage.h"

#include "wizardpage_p.h"

#ifdef QGPGME_SUPPORTS_SECRET_KEY_EXPORT
# include "commands/exportsecretkeycommand.h"
#else
# include "commands/exportsecretkeycommand_old.h"
#endif
#include "commands/exportopenpgpcertstoservercommand.h"
#include "commands/exportcertificatecommand.h"

#include "kleopatraapplication.h"

#include "utils/validation.h"
#include "utils/filedialog.h"
#include "utils/keyparameters.h"
#include "utils/userinfo.h"

#include <Libkleo/Compat>
#include <Libkleo/GnuPG>
#include <Libkleo/Stl_Util>
#include <Libkleo/Dn>
#include <Libkleo/OidMap>
#include <Libkleo/KeyCache>
#include <Libkleo/Formatting>

#include <QGpgME/KeyGenerationJob>
#include <QGpgME/Protocol>
#include <QGpgME/CryptoConfig>

#include <gpgme++/global.h>
#include <gpgme++/keygenerationresult.h>
#include <gpgme++/context.h>
#include <gpgme++/interfaces/passphraseprovider.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include "kleopatra_debug.h"
#include <QTemporaryDir>
#include <KMessageBox>
#include <QIcon>

#include <QRegularExpressionValidator>
#include <QLineEdit>
#include <QMetaProperty>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QDesktopServices>
#include <QUrlQuery>

#include <algorithm>

#include <KSharedConfig>
#include <QLocale>

using namespace Kleo;
using namespace Kleo::NewCertificateUi;
using namespace Kleo::Commands;
using namespace GpgME;
#ifndef QGPGME_SUPPORTS_SECRET_KEY_EXPORT
using Kleo::Commands::Compat::ExportSecretKeyCommand;
#endif

namespace
{

class ResultPage : public WizardPage
{
    Q_OBJECT
public:
    explicit ResultPage(QWidget *p = nullptr)
        : WizardPage(p),
          initialized(false),
          successfullyCreatedSigningCertificate(false),
          successfullyCreatedEncryptionCertificate(false),
          ui()
    {
        ui.setupUi(this);
        ui.dragQueen->setPixmap(QIcon::fromTheme(QStringLiteral("kleopatra")).pixmap(64, 64));
        registerField(QStringLiteral("error"),  ui.errorTB,   "plainText");
        registerField(QStringLiteral("result"), ui.resultTB,  "plainText");
        registerField(QStringLiteral("url"),    ui.dragQueen, "url");
        // hidden field, since QWizard can't deal with non-widget-backed fields...
        auto le = new QLineEdit(this);
        le->hide();
        registerField(QStringLiteral("fingerprint"), le);
    }

    void initializePage() override {
        const bool error = isError();

        if (error)
        {
            setTitle(i18nc("@title", "Key Creation Failed"));
            setSubTitle(i18n("Key pair creation failed. Please find details about the failure below."));
        } else {
            setTitle(i18nc("@title", "Key Pair Successfully Created"));
            setSubTitle(i18n("Your new key pair was created successfully. Please find details on the result and some suggested next steps below."));
        }

        ui.resultTB                 ->setVisible(!error);
        ui.errorTB                  ->setVisible(error);
        ui.dragQueen                ->setVisible(!error &&!pgp());
        ui.restartWizardPB          ->setVisible(error);
        ui.nextStepsGB              ->setVisible(!error);
        ui.saveRequestToFilePB      ->setVisible(!pgp());
        ui.makeBackupPB             ->setVisible(pgp());
        ui.createRevocationRequestPB->setVisible(pgp() &&false);     // not implemented

        ui.sendCertificateByEMailPB ->setVisible(pgp());
        ui.sendRequestByEMailPB     ->setVisible(!pgp());
        ui.uploadToKeyserverPB      ->setVisible(pgp());

        if (!error && !pgp())
        {
            if (signingAllowed() && !encryptionAllowed()) {
                successfullyCreatedSigningCertificate = true;
            } else if (!signingAllowed() && encryptionAllowed()) {
                successfullyCreatedEncryptionCertificate = true;
            } else {
                successfullyCreatedEncryptionCertificate = successfullyCreatedSigningCertificate = true;
            }
        }

        ui.createSigningCertificatePB->setVisible(successfullyCreatedEncryptionCertificate &&!successfullyCreatedSigningCertificate);
        ui.createEncryptionCertificatePB->setVisible(successfullyCreatedSigningCertificate &&!successfullyCreatedEncryptionCertificate);

        if (error) {
            wizard()->setOptions(wizard()->options() & ~QWizard::NoCancelButtonOnLastPage);
        } else {
            wizard()->setOptions(wizard()->options() | QWizard::NoCancelButtonOnLastPage);
        }

        if (!initialized) {
            connect(ui.restartWizardPB, &QAbstractButton::clicked,
                    this, [this]() {
                        restartAtEnterDetailsPage();
                    });
        }
        initialized = true;
    }

    bool isError() const
    {
        return !ui.errorTB->document()->isEmpty();
    }

    bool isComplete() const override
    {
        return !isError();
    }

private:
    Key key() const
    {
        return KeyCache::instance()->findByFingerprint(fingerprint().toLatin1().constData());
    }

private Q_SLOTS:
    void slotSaveRequestToFile()
    {
        QString fileName = FileDialog::getSaveFileName(this, i18nc("@title", "Save Request"),
                           QStringLiteral("imp"), i18n("PKCS#10 Requests (*.p10)"));
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
                                      src.fileName(), fileName, src.errorString()),
                               i18nc("@title", "Error Saving Request"));
        else
            KMessageBox::information(this,
                                     xi18nc("@info",
                                            "<para>Successfully wrote request to <filename>%1</filename>.</para>"
                                            "<para>You should now send the request to the Certification Authority (CA).</para>",
                                            fileName),
                                     i18nc("@title", "Request Saved"));
    }

    void slotSendRequestByEMail()
    {
        if (pgp()) {
            return;
        }
        const KConfigGroup config(KSharedConfig::openConfig(), "CertificateCreationWizard");
        invokeMailer(config.readEntry("CAEmailAddress"),    // to
                     i18n("Please process this certificate."), // subject
                     i18n("Please process this certificate and inform the sender about the location to fetch the resulting certificate.\n\nThanks,\n"), // body
                     QUrl(url()).toLocalFile());    // attachment
    }

    void slotSendCertificateByEMail()
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

    void slotSendCertificateByEMailContinuation()
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
        invokeMailer(QString(),  // to
                     i18n("My new public OpenPGP key"), // subject
                     i18n("Please find attached my new public OpenPGP key."), // body
                     fileName);
    }

    QByteArray ol_quote(QByteArray str)
    {
#ifdef Q_OS_WIN
        return "\"\"" + str.replace('"', "\\\"") + "\"\"";
        //return '"' + str.replace( '"', "\\\"" ) + '"';
#else
        return str;
#endif
    }

    void invokeMailer(const QString &to, const QString &subject, const QString &body, const QString &attachment)
    {
        qCDebug(KLEOPATRA_LOG) << "to:" << to << "subject:" << subject
                               << "body:" << body << "attachment:" << attachment;

        // RFC 2368 says body's linebreaks need to be encoded as
        // "%0D%0A", so normalize body to CRLF:
        //body.replace(QLatin1Char('\n'), QStringLiteral("\r\n")).remove(QStringLiteral("\r\r"));

        QUrlQuery query;
        query.addQueryItem(QStringLiteral("subject"), subject);
        query.addQueryItem(QStringLiteral("body"), body);
        if (!attachment.isEmpty()) {
            query.addQueryItem(QStringLiteral("attach"), attachment);
        }
        QUrl url;
        url.setScheme(QStringLiteral("mailto"));
        url.setQuery(query);
        qCDebug(KLEOPATRA_LOG) << "openUrl" << url;
        QDesktopServices::openUrl(url);
        KMessageBox::information(this,
                                 xi18nc("@info",
                                        "<para><application>Kleopatra</application> tried to send a mail via your default mail client.</para>"
                                        "<para>Some mail clients are known not to support attachments when invoked this way.</para>"
                                        "<para>If your mail client does not have an attachment, then drag the <application>Kleopatra</application> icon and drop it on the message compose window of your mail client.</para>"
                                        "<para>If that does not work, either, save the request to a file, and then attach that.</para>"),
                                 i18nc("@title", "Sending Mail"),
                                 QStringLiteral("newcertificatewizard-mailto-troubles"));
    }

    void slotUploadCertificateToDirectoryServer()
    {
        if (pgp()) {
            (new ExportOpenPGPCertsToServerCommand(key()))->start();
        }
    }

    void slotBackupCertificate()
    {
        if (pgp()) {
            (new ExportSecretKeyCommand(key()))->start();
        }
    }

    void slotCreateRevocationRequest()
    {

    }

    void slotCreateSigningCertificate()
    {
        if (successfullyCreatedSigningCertificate) {
            return;
        }
        toggleSignEncryptAndRestart();
    }

    void slotCreateEncryptionCertificate()
    {
        if (successfullyCreatedEncryptionCertificate) {
            return;
        }
        toggleSignEncryptAndRestart();
    }

private:
    void toggleSignEncryptAndRestart()
    {
        if (!wizard()) {
            return;
        }
        if (KMessageBox::warningContinueCancel(
                    this,
                    i18nc("@info",
                          "This operation will delete the certification request. "
                          "Please make sure that you have sent or saved it before proceeding."),
                    i18nc("@title", "Certification Request About To Be Deleted")) != KMessageBox::Continue) {
            return;
        }
        const bool sign = signingAllowed();
        const bool encr = encryptionAllowed();
        setField(QStringLiteral("signingAllowed"),    !sign);
        setField(QStringLiteral("encryptionAllowed"), !encr);
        restartAtEnterDetailsPage();
    }

private:
    bool initialized : 1;
    bool successfullyCreatedSigningCertificate : 1;
    bool successfullyCreatedEncryptionCertificate : 1;
    QPointer<ExportCertificateCommand> exportCertificateCommand;
    Ui_ResultPage ui;
};
}

class NewCertificateWizard::Private
{
    friend class ::Kleo::NewCertificateWizard;
    NewCertificateWizard *const q;
public:
    explicit Private(NewCertificateWizard *qq)
        : q(qq),
          tmp(QDir::temp().absoluteFilePath(QStringLiteral("kleo-"))),
          ui(q)
    {
        q->setWindowTitle(i18nc("@title:window", "Key Pair Creation Wizard"));
    }

private:
    GpgME::Protocol initialProtocol = GpgME::UnknownProtocol;
    QTemporaryDir tmp;
    struct Ui {
        ChooseProtocolPage chooseProtocolPage;
        EnterDetailsPage enterDetailsPage;
        KeyCreationPage keyCreationPage;
        ResultPage resultPage;

        explicit Ui(NewCertificateWizard *q)
            : chooseProtocolPage(q),
              enterDetailsPage(q),
              keyCreationPage(q),
              resultPage(q)
        {
            KDAB_SET_OBJECT_NAME(chooseProtocolPage);
            KDAB_SET_OBJECT_NAME(enterDetailsPage);
            KDAB_SET_OBJECT_NAME(keyCreationPage);
            KDAB_SET_OBJECT_NAME(resultPage);

            q->setOptions(NoBackButtonOnStartPage|DisabledBackButtonOnLastPage);

            q->setPage(ChooseProtocolPageId, &chooseProtocolPage);
            q->setPage(EnterDetailsPageId,   &enterDetailsPage);
            q->setPage(KeyCreationPageId,    &keyCreationPage);
            q->setPage(ResultPageId,         &resultPage);

            q->setStartId(ChooseProtocolPageId);
        }

    } ui;

};

NewCertificateWizard::NewCertificateWizard(QWidget *p)
    : QWizard(p), d(new Private(this))
{
}

NewCertificateWizard::~NewCertificateWizard() {}

void NewCertificateWizard::showEvent(QShowEvent *event)
{
    // set WA_KeyboardFocusChange attribute to force visual focus of the
    // focussed button when the wizard is shown (required for Breeze style
    // and some other styles)
    window()->setAttribute(Qt::WA_KeyboardFocusChange);
    QWizard::showEvent(event);
}

void NewCertificateWizard::setProtocol(Protocol proto)
{
    d->initialProtocol = proto;
    d->ui.chooseProtocolPage.setProtocol(proto);
    setStartId(proto == UnknownProtocol ? ChooseProtocolPageId : EnterDetailsPageId);
}

Protocol NewCertificateWizard::protocol() const
{
    return d->ui.chooseProtocolPage.protocol();
}

void NewCertificateWizard::resetProtocol()
{
    d->ui.chooseProtocolPage.setProtocol(d->initialProtocol);
}

void NewCertificateWizard::restartAtEnterDetailsPage()
{
    const auto protocol = d->ui.chooseProtocolPage.protocol();
    restart();  // resets the protocol to the initial protocol
    d->ui.chooseProtocolPage.setProtocol(protocol);
    while (currentId() != NewCertificateWizard::EnterDetailsPageId) {
        next();
    }
}

QDir NewCertificateWizard::tmpDir() const
{
    return QDir(d->tmp.path());
}

namespace
{
template <typename T = QString>
struct Row {
    QString key;
    T value;

    Row(const QString &k, const T &v) : key(k), value(v) {}
};
template <typename T>
QTextStream &operator<<(QTextStream &s, const Row<T> &row)
{
    if (row.key.isEmpty()) {
        return s;
    } else {
        return s << "<tr><td>" << row.key << "</td><td>" << row.value << "</td></tr>";
    }
}
}

#include "newcertificatewizard.moc"
