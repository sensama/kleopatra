/*
    main.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2001, 2002, 2004, 2008 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "aboutdata.h"
#include "kleopatraapplication.h"
#include "mainwindow.h"

#include "accessibility/accessiblewidgetfactory.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <Kdelibs4ConfigMigrator>
#endif

#include <commands/reloadkeyscommand.h>
#include <commands/selftestcommand.h>

#include <Libkleo/GnuPG>
#include <utils/archivedefinition.h>
#include "utils/kuniqueservice.h"
#include "utils/userinfo.h"

#include <uiserver/uiserver.h>
#include <uiserver/assuancommand.h>
#include <uiserver/echocommand.h>
#include <uiserver/decryptcommand.h>
#include <uiserver/verifycommand.h>
#include <uiserver/decryptverifyfilescommand.h>
#include <uiserver/decryptfilescommand.h>
#include <uiserver/verifyfilescommand.h>
#include <uiserver/prepencryptcommand.h>
#include <uiserver/prepsigncommand.h>
#include <uiserver/encryptcommand.h>
#include <uiserver/signcommand.h>
#include <uiserver/signencryptfilescommand.h>
#include <uiserver/selectcertificatecommand.h>
#include <uiserver/importfilescommand.h>
#include <uiserver/createchecksumscommand.h>
#include <uiserver/verifychecksumscommand.h>

#include <Libkleo/ChecksumDefinition>

#include "kleopatra_debug.h"
#include "kleopatra_options.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KCrash>

#include <QAccessible>
#include <QTextDocument> // for Qt::escape
#include <QMessageBox>
#include <QTimer>
#include <QTime>
#include <QEventLoop>
#include <QThreadPool>
#include <QElapsedTimer>

#include <gpgme++/global.h>
#include <gpgme++/error.h>

#include <memory>
#include <iostream>
#include <QCommandLineParser>

static bool selfCheck()
{
    Kleo::Commands::SelfTestCommand cmd(nullptr);
    cmd.setAutoDelete(false);
    cmd.setAutomaticMode(true);
    QEventLoop loop;
    QObject::connect(&cmd, &Kleo::Commands::SelfTestCommand::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(0, &cmd, &Kleo::Command::start);   // start() may Q_EMIT finished()...
    loop.exec();
    if (cmd.isCanceled()) {
        return false;
    } else {
        return true;
    }
}

static void fillKeyCache(Kleo::UiServer *server)
{
    auto cmd = new Kleo::ReloadKeysCommand(nullptr);
    QObject::connect(cmd, SIGNAL(finished()), server, SLOT(enableCryptoCommands()));
    cmd->start();
}

int main(int argc, char **argv)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif
    KleopatraApplication app(argc, argv);
    KCrash::initialize();
    QAccessible::installFactory(Kleo::accessibleWidgetFactory);

    QElapsedTimer timer;
    timer.start();

    // Initialize GpgME
    {
        const GpgME::Error gpgmeInitError = GpgME::initializeLibrary(0);
        if (gpgmeInitError) {
            KMessageBox::error(nullptr, xi18nc("@info",
                                        "<para>The version of the <application>GpgME</application> library you are running against "
                                        "is older than the one that the <application>GpgME++</application> library was built against.</para>"
                                        "<para><application>Kleopatra</application> will not function in this setting.</para>"
                                        "<para>Please ask your administrator for help in resolving this issue.</para>"),
                            i18nc("@title", "GpgME Too Old"));
            return EXIT_FAILURE;
        }
        qCDebug(KLEOPATRA_LOG) << "Startup timing:" << timer.elapsed() << "ms elapsed: GPGME Initialized";
    }

    KLocalizedString::setApplicationDomain("kleopatra");

    AboutData aboutData;
    KAboutData::setApplicationData(aboutData);

    if (Kleo::userIsElevated()) {
        /* This is a safeguard against bugreports that something fails because
         * of permission problems on windows.  Some users still have the Windows
         * Vista behavior of running things as Administrator.  This can break
         * GnuPG in horrible ways for example if a stale lockfile is left that
         * can't be removed without another elevation.
         *
         * Note: This is not the same as running as root on Linux. Elevated means
         * that you are temporarily running with the "normal" user environment but
         * with elevated permissions.
         * */
        if (KMessageBox::warningContinueCancel(nullptr, xi18nc("@info",
                                               "<para><application>Kleopatra</application> cannot be run as adminstrator without "
                                               "breaking file permissions in the GnuPG data folder.</para>"
                                               "<para>To manage keys for other users please manage them as a normal user and "
                                               "copy the <filename>AppData\\Roaming\\gnupg</filename> directory with proper permissions.</para>") +
                                               xi18n("<para>Are you sure that you want to continue?</para>"),
                                               i18nc("@title", "Running as Administrator")) != KMessageBox::Continue) {
            return EXIT_FAILURE;
        }
        qCWarning(KLEOPATRA_LOG) << "User is running with administrative permissions.";
    }

    KUniqueService service;
    QObject::connect(&service, &KUniqueService::activateRequested,
                     &app, &KleopatraApplication::slotActivateRequested);
    QObject::connect(&app, &KleopatraApplication::setExitValue,
    &service, [&service](int i) {
        service.setExitValue(i);
    });
    // Delay init after KUniqueservice call as this might already
    // have terminated us and so we can avoid overhead (e.g. keycache
    // setup / systray icon).
    qCDebug(KLEOPATRA_LOG) << "Startup timing:" << timer.elapsed() << "ms elapsed: Service created";
    app.init();
    qCDebug(KLEOPATRA_LOG) << "Startup timing:" << timer.elapsed() << "ms elapsed: Application initialized";

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    kleopatra_options(&parser);

    parser.process(QApplication::arguments());
    aboutData.processCommandLine(&parser);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    Kdelibs4ConfigMigrator migrate(QStringLiteral("kleopatra"));
    migrate.setConfigFiles(QStringList() << QStringLiteral("kleopatrarc")
                                         << QStringLiteral("libkleopatrarc"));
    migrate.setUiFiles(QStringList() << QStringLiteral("kleopatra.rc"));
    migrate.migrate();
#endif
    qCDebug(KLEOPATRA_LOG) << "Startup timing:" << timer.elapsed() << "ms elapsed: Application created";

    {
        const unsigned int threads = QThreadPool::globalInstance()->maxThreadCount();
        QThreadPool::globalInstance()->setMaxThreadCount(qMax(2U, threads));
    }

    Kleo::ChecksumDefinition::setInstallPath(Kleo::gpg4winInstallPath());
    Kleo::ArchiveDefinition::setInstallPath(Kleo::gnupgInstallPath());

    int rc;
    Kleo::UiServer server(parser.value(QStringLiteral("uiserver-socket")));
    try {
        qCDebug(KLEOPATRA_LOG) << "Startup timing:" << timer.elapsed() << "ms elapsed: UiServer created";

        QObject::connect(&server, &Kleo::UiServer::startKeyManagerRequested, &app, &KleopatraApplication::openOrRaiseMainWindow);

        QObject::connect(&server, &Kleo::UiServer::startConfigDialogRequested, &app, &KleopatraApplication::openOrRaiseConfigDialog);

#define REGISTER( Command ) server.registerCommandFactory( std::shared_ptr<Kleo::AssuanCommandFactory>( new Kleo::GenericAssuanCommandFactory<Kleo::Command> ) )
        REGISTER(CreateChecksumsCommand);
        REGISTER(DecryptCommand);
        REGISTER(DecryptFilesCommand);
        REGISTER(DecryptVerifyFilesCommand);
        REGISTER(EchoCommand);
        REGISTER(EncryptCommand);
        REGISTER(EncryptFilesCommand);
        REGISTER(EncryptSignFilesCommand);
        REGISTER(ImportFilesCommand);
        REGISTER(PrepEncryptCommand);
        REGISTER(PrepSignCommand);
        REGISTER(SelectCertificateCommand);
        REGISTER(SignCommand);
        REGISTER(SignEncryptFilesCommand);
        REGISTER(SignFilesCommand);
        REGISTER(VerifyChecksumsCommand);
        REGISTER(VerifyCommand);
        REGISTER(VerifyFilesCommand);
#undef REGISTER

        server.start();
        qCDebug(KLEOPATRA_LOG) << "Startup timing:" << timer.elapsed() << "ms elapsed: UiServer started";
    } catch (const std::exception &e) {
        qCDebug(KLEOPATRA_LOG) << "Failed to start UI Server: " << e.what();
#ifdef Q_OS_WIN
        // Once there actually is a plugin for other systems then Windows this
        // error should probably be shown, too. But currently only Windows users need
        // to care.
        QMessageBox::information(nullptr, i18n("GPG UI Server Error"),
                                 i18n("<qt>The Kleopatra GPG UI Server Module could not be initialized.<br/>"
                                      "The error given was: <b>%1</b><br/>"
                                      "You can use Kleopatra as a certificate manager, but cryptographic plugins that "
                                      "rely on a GPG UI Server being present might not work correctly, or at all.</qt>",
                                      QString::fromUtf8(e.what()).toHtmlEscaped()));
#endif
    }
    const bool daemon = parser.isSet(QStringLiteral("daemon"));
    if (!daemon && app.isSessionRestored()) {
        app.restoreMainWindow();
    }

    if (!selfCheck()) {
        return EXIT_FAILURE;
    }
    qCDebug(KLEOPATRA_LOG) << "Startup timing:" << timer.elapsed() << "ms elapsed: SelfCheck completed";

    fillKeyCache(&server);
#ifndef QT_NO_SYSTEMTRAYICON
    app.startMonitoringSmartCard();
#endif
    app.setIgnoreNewInstance(false);

    if (!daemon) {
        const QString err = app.newInstance(parser);
        if (!err.isEmpty()) {
            std::cerr << i18n("Invalid arguments: %1", err).toLocal8Bit().constData() << "\n";
            return EXIT_FAILURE;
        }
        qCDebug(KLEOPATRA_LOG) << "Startup timing:" << timer.elapsed() << "ms elapsed: new instance created";
    }

    rc = app.exec();

    app.setIgnoreNewInstance(true);
    QObject::disconnect(&server, &Kleo::UiServer::startKeyManagerRequested, &app, &KleopatraApplication::openOrRaiseMainWindow);
    QObject::disconnect(&server, &Kleo::UiServer::startConfigDialogRequested, &app, &KleopatraApplication::openOrRaiseConfigDialog);

    server.stop();
    server.waitForStopped();

    return rc;
}
