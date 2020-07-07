/*
    kleopatraapplication.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB
    2016 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

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

#include "kleopatraapplication.h"

#include "mainwindow.h"
#include "kleopatra_options.h"
#include "systrayicon.h"
#include <smartcard/readerstatus.h>
#include <conf/configuredialog.h>

#include <utils/gnupg-helper.h>
#include <utils/kdpipeiodevice.h>
#include <utils/log.h>

#include <gpgme++/key.h>

#include <Libkleo/FileSystemWatcher>
#include <Libkleo/KeyCache>
#include <Libkleo/Classify>

#ifdef HAVE_USABLE_ASSUAN
# include <uiserver/uiserver.h>
#endif

#include "commands/signencryptfilescommand.h"
#include "commands/decryptverifyfilescommand.h"
#include "commands/lookupcertificatescommand.h"
#include "commands/checksumcreatefilescommand.h"
#include "commands/checksumverifyfilescommand.h"
#include "commands/detailscommand.h"
#include "commands/newcertificatecommand.h"

#include "dialogs/updatenotification.h"

#include <KIconLoader>
#include <KLocalizedString>
#include "kleopatra_debug.h"
#include <KMessageBox>
#include <KWindowSystem>

#include <QFile>
#include <QDir>
#include <QPointer>

#include <memory>
#include <KSharedConfig>

using namespace Kleo;
using namespace Kleo::Commands;

static void add_resources()
{
    KIconLoader::global()->addAppDir(QStringLiteral("libkleopatra"));
    KIconLoader::global()->addAppDir(QStringLiteral("kwatchgnupg"));
}

static QList<QByteArray> default_logging_options()
{
    QList<QByteArray> result;
    result.push_back("io");
    return result;
}

class KleopatraApplication::Private
{
    friend class ::KleopatraApplication;
    KleopatraApplication *const q;
public:
    explicit Private(KleopatraApplication *qq)
        : q(qq),
          ignoreNewInstance(true),
          firstNewInstance(true),
          sysTray(nullptr)
    {
    }
    ~Private() {
#ifndef QT_NO_SYSTEMTRAYICON
        delete sysTray;
#endif
    }
    void init()
    {
        KDAB_SET_OBJECT_NAME(readerStatus);
#ifndef QT_NO_SYSTEMTRAYICON
        sysTray = new SysTrayIcon();
        sysTray->setAnyCardHasNullPin(readerStatus.anyCardHasNullPin());
        sysTray->setAnyCardCanLearnKeys(readerStatus.anyCardCanLearnKeys());

        connect(&readerStatus, &SmartCard::ReaderStatus::anyCardHasNullPinChanged,
                sysTray, &SysTrayIcon::setAnyCardHasNullPin);
        connect(&readerStatus, &SmartCard::ReaderStatus::anyCardCanLearnKeysChanged,
                sysTray, &SysTrayIcon::setAnyCardCanLearnKeys);
#endif
    }

private:
    void connectConfigureDialog()
    {
        if (configureDialog && q->mainWindow()) {
            connect(configureDialog, SIGNAL(configCommitted()), q->mainWindow(), SLOT(slotConfigCommitted()));
        }
    }
    void disconnectConfigureDialog()
    {
        if (configureDialog && q->mainWindow()) {
            disconnect(configureDialog, SIGNAL(configCommitted()), q->mainWindow(), SLOT(slotConfigCommitted()));
        }
    }

public:
    bool ignoreNewInstance;
    bool firstNewInstance;
    QPointer<ConfigureDialog> configureDialog;
    QPointer<MainWindow> mainWindow;
    SmartCard::ReaderStatus readerStatus;
#ifndef QT_NO_SYSTEMTRAYICON
    SysTrayIcon *sysTray;
#endif
    std::shared_ptr<KeyCache> keyCache;
    std::shared_ptr<Log> log;
    std::shared_ptr<FileSystemWatcher> watcher;

public:
    void setupKeyCache()
    {
        keyCache = KeyCache::mutableInstance();
        watcher.reset(new FileSystemWatcher);

        watcher->whitelistFiles(gnupgFileWhitelist());
        watcher->addPath(gnupgHomeDirectory());
        watcher->setDelay(1000);
        keyCache->addFileSystemWatcher(watcher);
    }

    void setupLogging()
    {
        log = Log::mutableInstance();

        const QByteArray envOptions = qgetenv("KLEOPATRA_LOGOPTIONS");
        const bool logAll = envOptions.trimmed() == "all";
        const QList<QByteArray> options = envOptions.isEmpty() ? default_logging_options() : envOptions.split(',');

        const QByteArray dirNative = qgetenv("KLEOPATRA_LOGDIR");
        if (dirNative.isEmpty()) {
            return;
        }
        const QString dir = QFile::decodeName(dirNative);
        const QString logFileName = QDir(dir).absoluteFilePath(QStringLiteral("kleopatra.log.%1").arg(QCoreApplication::applicationPid()));
        std::unique_ptr<QFile> logFile(new QFile(logFileName));
        if (!logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
            qCDebug(KLEOPATRA_LOG) << "Could not open file for logging: " << logFileName << "\nLogging disabled";
            return;
        }

        log->setOutputDirectory(dir);
        if (logAll || options.contains("io")) {
            log->setIOLoggingEnabled(true);
        }
        qInstallMessageHandler(Log::messageHandler);

#ifdef HAVE_USABLE_ASSUAN
        if (logAll || options.contains("pipeio")) {
            KDPipeIODevice::setDebugLevel(KDPipeIODevice::Debug);
        }
        UiServer::setLogStream(log->logFile());
#endif

    }
};

KleopatraApplication::KleopatraApplication(int &argc, char *argv[])
    : QApplication(argc, argv), d(new Private(this))
{
}

void KleopatraApplication::init()
{
    d->init();
    add_resources();
    d->setupKeyCache();
    d->setupLogging();
#ifndef QT_NO_SYSTEMTRAYICON
    d->sysTray->show();
#endif
    setQuitOnLastWindowClosed(false);
    KWindowSystem::allowExternalProcessWindowActivation();
}

KleopatraApplication::~KleopatraApplication()
{
    // main window doesn't receive "close" signal and cannot
    // save settings before app exit
    delete d->mainWindow;

    // work around kdelibs bug https://bugs.kde.org/show_bug.cgi?id=162514
    KSharedConfig::openConfig()->sync();
}

namespace
{
typedef void (KleopatraApplication::*Func)(const QStringList &, GpgME::Protocol);
}

void KleopatraApplication::slotActivateRequested(const QStringList &arguments,
        const QString &workingDirectory)
{
    QCommandLineParser parser;

    kleopatra_options(&parser);
    QString err;
    if (!arguments.isEmpty() && !parser.parse(arguments)) {
        err = parser.errorText();
    } else if (arguments.isEmpty()) {
        // KDBusServices omits the application name if no other
        // arguments are provided. In that case the parser prints
        // a warning.
        parser.parse(QStringList() << QCoreApplication::applicationFilePath());
    }

    if (err.isEmpty()) {
        err = newInstance(parser, workingDirectory);
    }

    if (!err.isEmpty()) {
        KMessageBox::sorry(nullptr, err.toHtmlEscaped(), i18n("Failed to execute command"));
        Q_EMIT setExitValue(1);
        return;
    }
    Q_EMIT setExitValue(0);
}

QString KleopatraApplication::newInstance(const QCommandLineParser &parser,
        const QString &workingDirectory)
{
    if (d->ignoreNewInstance) {
        qCDebug(KLEOPATRA_LOG) << "New instance ignored because of ignoreNewInstance";
        return QString();
    }

    QStringList files;
    const QDir cwd = QDir(workingDirectory);
    bool queryMode = parser.isSet(QStringLiteral("query")) || parser.isSet(QStringLiteral("search"));

    // Query and Search treat positional arguments differently, see below.
    if (!queryMode) {
        Q_FOREACH (const QString &file, parser.positionalArguments()) {
            // We do not check that file exists here. Better handle
            // these errors in the UI.
            if (QFileInfo(file).isAbsolute()) {
                files << file;
            } else {
                files << cwd.absoluteFilePath(file);
            }
        }
    }

    GpgME::Protocol protocol = GpgME::UnknownProtocol;

    if (parser.isSet(QStringLiteral("openpgp"))) {
        qCDebug(KLEOPATRA_LOG) << "found OpenPGP";
        protocol = GpgME::OpenPGP;
    }

    if (parser.isSet(QStringLiteral("cms"))) {
        qCDebug(KLEOPATRA_LOG) << "found CMS";
        if (protocol == GpgME::OpenPGP) {
            return i18n("Ambiguous protocol: --openpgp and --cms");
        }
        protocol = GpgME::CMS;
    }

    // Check for Parent Window id
    WId parentId = 0;
    if (parser.isSet(QStringLiteral("parent-windowid"))) {
#ifdef Q_OS_WIN
        // WId is not a portable type as it is a pointer type on Windows.
        // casting it from an integer is ok though as the values are guaranteed to
        // be compatible in the documentation.
        parentId = reinterpret_cast<WId>(parser.value(QStringLiteral("parent-windowid")).toUInt());
#else
        parentId = parser.value(QStringLiteral("parent-windowid")).toUInt();
#endif
    }

    // Handle openpgp4fpr URI scheme
    QString needle;
    if (queryMode) {
        needle = parser.positionalArguments().join(QLatin1Char(' '));
    }
    if (needle.startsWith(QLatin1String("openpgp4fpr:"))) {
        needle.remove(0, 12);
    }

    // Check for --search command.
    if (parser.isSet(QStringLiteral("search"))) {
        // This is an extra command instead of a combination with the
        // similar query to avoid changing the older query commands behavior
        // and query's "show details if a certificate exist or search on a
        // keyserver" logic is hard to explain and use consistently.
        if (needle.isEmpty()) {
            return i18n("No search string specified for --search");
        }
        LookupCertificatesCommand *const cmd = new LookupCertificatesCommand(needle, nullptr);
        cmd->setParentWId(parentId);
        cmd->start();
        return QString();
    }

    // Check for --query command
    if (parser.isSet(QStringLiteral("query"))) {
        if (needle.isEmpty()) {
            return i18n("No fingerprint argument specified for --query");
        }
        auto cmd = Command::commandForQuery(needle);
        cmd->setParentWId(parentId);
        cmd->start();
        return QString();
    }

    // Check for --gen-key command
    if (parser.isSet(QStringLiteral("gen-key"))) {
        auto cmd = new NewCertificateCommand(nullptr);
        cmd->setParentWId(parentId);
        cmd->setProtocol(protocol);
        cmd->start();
        return QString();
    }

    // Check for --config command
    if (parser.isSet(QStringLiteral("config"))) {
        openConfigDialogWithForeignParent(parentId);
        return QString();
    }

    static const QMap<QString, Func> funcMap {
        { QStringLiteral("import-certificate"), &KleopatraApplication::importCertificatesFromFile },
        { QStringLiteral("encrypt"), &KleopatraApplication::encryptFiles },
        { QStringLiteral("sign"), &KleopatraApplication::signFiles },
        { QStringLiteral("encrypt-sign"), &KleopatraApplication::signEncryptFiles },
        { QStringLiteral("sign-encrypt"), &KleopatraApplication::signEncryptFiles },
        { QStringLiteral("decrypt"), &KleopatraApplication::decryptFiles },
        { QStringLiteral("verify"), &KleopatraApplication::verifyFiles },
        { QStringLiteral("decrypt-verify"), &KleopatraApplication::decryptVerifyFiles },
        { QStringLiteral("checksum"), &KleopatraApplication::checksumFiles },
    };

    QString found;
    Q_FOREACH (const QString &opt, funcMap.keys()) {
        if (parser.isSet(opt) && found.isEmpty()) {
            found = opt;
        } else if (parser.isSet(opt)) {
            return i18n("Ambiguous commands \"%1\" and \"%2\"", found, opt);
        }
    }

    QStringList errors;
    if (!found.isEmpty()) {
        if (files.empty()) {
            return i18n("No files specified for \"%1\" command", found);
        }
        qCDebug(KLEOPATRA_LOG) << "found" << found;
        (this->*funcMap.value(found))(files, protocol);
    } else {
        if (files.empty()) {
            if (!(d->firstNewInstance && isSessionRestored())) {
                qCDebug(KLEOPATRA_LOG) << "openOrRaiseMainWindow";
                openOrRaiseMainWindow();
            }
        } else {
            Q_FOREACH (const QString& fileName, files) {
                QFileInfo fi(fileName);
                if (!fi.isReadable()) {
                    errors << i18n("Cannot read \"%1\"", fileName);
                }
            }
            Q_FOREACH (Command *cmd, Command::commandsForFiles(files)) {
                if (parentId) {
                    cmd->setParentWId(parentId);
                } else {
                    MainWindow *mw = mainWindow();
                    if (!mw) {
                        mw = new MainWindow;
                        mw->setAttribute(Qt::WA_DeleteOnClose);
                        setMainWindow(mw);
                        d->connectConfigureDialog();
                    }
                    cmd->setParentWidget(mw);
                }
                cmd->start();
            }
        }
    }
    d->firstNewInstance = false;

#ifdef Q_OS_WIN
    // On Windows we might be started from the
    // explorer in any working directory. E.g.
    // a double click on a file. To avoid preventing
    // the folder from deletion we set the
    // working directory to the users homedir.
    QDir::setCurrent(QDir::homePath());
#endif

    return errors.join(QLatin1Char('\n'));
}

#ifndef QT_NO_SYSTEMTRAYICON
const SysTrayIcon *KleopatraApplication::sysTrayIcon() const
{
    return d->sysTray;
}

SysTrayIcon *KleopatraApplication::sysTrayIcon()
{
    return d->sysTray;
}
#endif

const MainWindow *KleopatraApplication::mainWindow() const
{
    return d->mainWindow;
}

MainWindow *KleopatraApplication::mainWindow()
{
    return d->mainWindow;
}

void KleopatraApplication::setMainWindow(MainWindow *mainWindow)
{
    if (mainWindow == d->mainWindow) {
        return;
    }

    d->disconnectConfigureDialog();

    d->mainWindow = mainWindow;
#ifndef QT_NO_SYSTEMTRAYICON
    d->sysTray->setMainWindow(mainWindow);
#endif

    d->connectConfigureDialog();
}

static void open_or_raise(QWidget *w)
{
    if (w->isMinimized()) {
        KWindowSystem::unminimizeWindow(w->winId());
        w->raise();
    } else if (w->isVisible()) {
        w->raise();
    } else {
        w->show();
    }
}

void KleopatraApplication::toggleMainWindowVisibility()
{
    if (mainWindow()) {
        mainWindow()->setVisible(!mainWindow()->isVisible());
    } else {
        openOrRaiseMainWindow();
    }
}

void KleopatraApplication::restoreMainWindow()
{
    qCDebug(KLEOPATRA_LOG) << "restoring main window";

    // Sanity checks
    if (!isSessionRestored()) {
        qCDebug(KLEOPATRA_LOG) << "Not in session restore";
        return;
    }

    if (mainWindow()) {
        qCDebug(KLEOPATRA_LOG) << "Already have main window";
        return;
    }

    MainWindow *mw = new MainWindow;
    if (KMainWindow::canBeRestored(1)) {
        // restore to hidden state, Mainwindow::readProperties() will
        // restore saved visibility.
        mw->restore(1, false);
    }

    mw->setAttribute(Qt::WA_DeleteOnClose);
    setMainWindow(mw);
    d->connectConfigureDialog();

}

void KleopatraApplication::openOrRaiseMainWindow()
{
    MainWindow *mw = mainWindow();
    if (!mw) {
        mw = new MainWindow;
        mw->setAttribute(Qt::WA_DeleteOnClose);
        setMainWindow(mw);
        d->connectConfigureDialog();
    }
    open_or_raise(mw);
    UpdateNotification::checkUpdate(mw);
}

void KleopatraApplication::openConfigDialogWithForeignParent(WId parentWId)
{
    if (!d->configureDialog) {
        d->configureDialog = new ConfigureDialog;
        d->configureDialog->setAttribute(Qt::WA_DeleteOnClose);
        d->connectConfigureDialog();
    }

    // This is similar to what the commands do.
    if (parentWId) {
        if (QWidget *pw = QWidget::find(parentWId)) {
            d->configureDialog->setParent(pw, d->configureDialog->windowFlags());
        } else {
            d->configureDialog->setAttribute(Qt::WA_NativeWindow, true);
            KWindowSystem::setMainWindow(d->configureDialog->windowHandle(), parentWId);
        }
    }

    open_or_raise(d->configureDialog);

    // If we have a parent we want to raise over it.
    if (parentWId) {
        d->configureDialog->raise();
    }
}

void KleopatraApplication::openOrRaiseConfigDialog()
{
    openConfigDialogWithForeignParent(0);
}

#ifndef QT_NO_SYSTEMTRAYICON
void KleopatraApplication::startMonitoringSmartCard()
{
    d->readerStatus.startMonitoring();
}
#endif // QT_NO_SYSTEMTRAYICON

void KleopatraApplication::importCertificatesFromFile(const QStringList &files, GpgME::Protocol /*proto*/)
{
    openOrRaiseMainWindow();
    if (!files.empty()) {
        mainWindow()->importCertificatesFromFile(files);
    }
}

void KleopatraApplication::encryptFiles(const QStringList &files, GpgME::Protocol proto)
{
    SignEncryptFilesCommand *const cmd = new SignEncryptFilesCommand(files, nullptr);
    cmd->setEncryptionPolicy(Force);
    cmd->setSigningPolicy(Allow);
    if (proto != GpgME::UnknownProtocol) {
        cmd->setProtocol(proto);
    }
    cmd->start();
}

void KleopatraApplication::signFiles(const QStringList &files, GpgME::Protocol proto)
{
    SignEncryptFilesCommand *const cmd = new SignEncryptFilesCommand(files, nullptr);
    cmd->setSigningPolicy(Force);
    cmd->setEncryptionPolicy(Deny);
    if (proto != GpgME::UnknownProtocol) {
        cmd->setProtocol(proto);
    }
    cmd->start();
}

void KleopatraApplication::signEncryptFiles(const QStringList &files, GpgME::Protocol proto)
{
    SignEncryptFilesCommand *const cmd = new SignEncryptFilesCommand(files, nullptr);
    if (proto != GpgME::UnknownProtocol) {
        cmd->setProtocol(proto);
    }
    cmd->start();
}

void KleopatraApplication::decryptFiles(const QStringList &files, GpgME::Protocol /*proto*/)
{
    DecryptVerifyFilesCommand *const cmd = new DecryptVerifyFilesCommand(files, nullptr);
    cmd->setOperation(Decrypt);
    cmd->start();
}

void KleopatraApplication::verifyFiles(const QStringList &files, GpgME::Protocol /*proto*/)
{
    DecryptVerifyFilesCommand *const cmd = new DecryptVerifyFilesCommand(files, nullptr);
    cmd->setOperation(Verify);
    cmd->start();
}

void KleopatraApplication::decryptVerifyFiles(const QStringList &files, GpgME::Protocol /*proto*/)
{
    DecryptVerifyFilesCommand *const cmd = new DecryptVerifyFilesCommand(files, nullptr);
    cmd->start();
}

void KleopatraApplication::checksumFiles(const QStringList &files, GpgME::Protocol /*proto*/)
{
    QStringList verifyFiles, createFiles;

    for (const QString &file : files) {
        if (isChecksumFile(file)) {
            verifyFiles << file;
        } else {
            createFiles << file;
        }
    }

    if (!verifyFiles.isEmpty()) {
        auto *const cmd = new ChecksumVerifyFilesCommand(verifyFiles, nullptr);
        cmd->start();
    }
    if (!createFiles.isEmpty()) {
        auto *const cmd = new ChecksumCreateFilesCommand(createFiles, nullptr);
        cmd->start();
    }
}

void KleopatraApplication::setIgnoreNewInstance(bool ignore)
{
    d->ignoreNewInstance = ignore;
}

bool KleopatraApplication::ignoreNewInstance() const
{
    return d->ignoreNewInstance;
}
