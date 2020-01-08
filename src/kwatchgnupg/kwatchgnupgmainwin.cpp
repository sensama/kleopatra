/*
    kwatchgnupgmainwin.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2001,2002,2004 Klar�vdalens Datakonsult AB

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

#include "kwatchgnupgmainwin.h"
#include "kwatchgnupgconfig.h"
#include "kwatchgnupg.h"
#include "tray.h"

#include <QGpgME/Protocol>
#include <QGpgME/CryptoConfig>

#include <QTextEdit>

#include <KMessageBox>
#include <KLocalizedString>
#include <QApplication>
#include <QAction>
#include <KActionCollection>
#include <KStandardAction>
#include <KProcess>
#include <KConfig>
#include <KEditToolBar>
#include <KShortcutsDialog>
#include <QIcon>
#include <KConfigGroup>

#include <QEventLoop>
#include <QTextStream>
#include <QDateTime>
#include <QFileDialog>
#include <KSharedConfig>

KWatchGnuPGMainWindow::KWatchGnuPGMainWindow(QWidget *parent)
    : KXmlGuiWindow(parent, Qt::Window), mConfig(nullptr)
{
    createActions();
    createGUI();

    mCentralWidget = new QTextEdit(this);
    mCentralWidget->setReadOnly(true);

    setCentralWidget(mCentralWidget);

    mWatcher = new KProcess;
    connect(mWatcher, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(slotWatcherExited(int,QProcess::ExitStatus)));

    connect(mWatcher, &QProcess::readyReadStandardOutput,
            this, &KWatchGnuPGMainWindow::slotReadStdout);

    slotReadConfig();
    mSysTray = new KWatchGnuPGTray(this);
    QAction *act = mSysTray->action(QStringLiteral("quit"));
    if (act) {
        connect(act, &QAction::triggered, this, &KWatchGnuPGMainWindow::slotQuit);
    }

    setAutoSaveSettings();
}

KWatchGnuPGMainWindow::~KWatchGnuPGMainWindow()
{
    delete mWatcher;
}

void KWatchGnuPGMainWindow::slotClear()
{
    mCentralWidget->clear();
    mCentralWidget->append(i18n("[%1] Log cleared", QDateTime::currentDateTime().toString(Qt::ISODate)));
}

void KWatchGnuPGMainWindow::createActions()
{
    QAction *action = actionCollection()->addAction(QStringLiteral("clear_log"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-history")));
    action->setText(i18n("C&lear History"));
    connect(action, &QAction::triggered, this, &KWatchGnuPGMainWindow::slotClear);
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL + Qt::Key_L));
    (void)KStandardAction::saveAs(this, SLOT(slotSaveAs()), actionCollection());
    (void)KStandardAction::close(this, SLOT(close()), actionCollection());
    (void)KStandardAction::quit(this, SLOT(slotQuit()), actionCollection());
    (void)KStandardAction::preferences(this, SLOT(slotConfigure()), actionCollection());
    (void)KStandardAction::keyBindings(this, SLOT(configureShortcuts()), actionCollection());
    (void)KStandardAction::configureToolbars(this, SLOT(slotConfigureToolbars()), actionCollection());
}

void KWatchGnuPGMainWindow::configureShortcuts()
{
    KShortcutsDialog::configure(actionCollection(), KShortcutsEditor::LetterShortcutsAllowed, this);
}

void KWatchGnuPGMainWindow::slotConfigureToolbars()
{
    KEditToolBar dlg(factory());
    dlg.exec();
}

void KWatchGnuPGMainWindow::startWatcher()
{
    disconnect(mWatcher, SIGNAL(finished(int,QProcess::ExitStatus)),
               this, SLOT(slotWatcherExited(int,QProcess::ExitStatus)));
    if (mWatcher->state() == QProcess::Running) {
        mWatcher->kill();
        while (mWatcher->state() == QProcess::Running) {
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        }
        mCentralWidget->append(i18n("[%1] Log stopped", QDateTime::currentDateTime().toString(Qt::ISODate)));
        mCentralWidget->ensureCursorVisible();
    }
    mWatcher->clearProgram();

    {
        const KConfigGroup config(KSharedConfig::openConfig(), "WatchGnuPG");
        *mWatcher << config.readEntry("Executable", WATCHGNUPGBINARY);
        *mWatcher << QStringLiteral("--force");
        *mWatcher << config.readEntry("Socket", WATCHGNUPGSOCKET);
    }

    mWatcher->setOutputChannelMode(KProcess::OnlyStdoutChannel);
    mWatcher->start();
    const bool ok = mWatcher->waitForStarted();
    if (!ok) {
        KMessageBox::sorry(this, i18n("The watchgnupg logging process could not be started.\nPlease install watchgnupg somewhere in your $PATH.\nThis log window is unable to display any useful information."));
    } else {
        mCentralWidget->append(i18n("[%1] Log started", QDateTime::currentDateTime().toString(Qt::ISODate)));
        mCentralWidget->ensureCursorVisible();
    }
    connect(mWatcher, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(slotWatcherExited(int,QProcess::ExitStatus)));
}

void KWatchGnuPGMainWindow::setGnuPGConfig()
{
    QStringList logclients;
    // Get config object
    QGpgME::CryptoConfig *cconfig = QGpgME::cryptoConfig();
    if (!cconfig) {
        return;
    }
    //Q_ASSERT( cconfig );
    KConfigGroup config(KSharedConfig::openConfig(), "WatchGnuPG");
    const QStringList comps = cconfig->componentList();
    for (QStringList::const_iterator it = comps.constBegin(); it != comps.constEnd(); ++it) {
        QGpgME::CryptoConfigComponent *comp = cconfig->component(*it);
        Q_ASSERT(comp);
        // Look for log-file entry in Debug group
        QGpgME::CryptoConfigGroup *group = comp->group(QStringLiteral("Debug"));
        if (group) {
            QGpgME::CryptoConfigEntry *entry = group->entry(QStringLiteral("log-file"));
            if (entry) {
                entry->setStringValue(QLatin1String("socket://") + config.readEntry("Socket", WATCHGNUPGSOCKET));
                logclients << QStringLiteral("%1 (%2)").arg(*it, comp->description());
            }
            entry = group->entry(QStringLiteral("debug-level"));
            if (entry) {
                entry->setStringValue(config.readEntry("LogLevel", "basic"));
            }
        }
    }
    cconfig->sync(true);
    if (logclients.isEmpty()) {
        KMessageBox::sorry(nullptr, i18n("There are no components available that support logging."));
    }
}

void KWatchGnuPGMainWindow::slotWatcherExited(int, QProcess::ExitStatus)
{
    if (KMessageBox::questionYesNo(this, i18n("The watchgnupg logging process died.\nDo you want to try to restart it?"), QString(), KGuiItem(i18n("Try Restart")), KGuiItem(i18n("Do Not Try"))) == KMessageBox::Yes) {
        mCentralWidget->append(i18n("====== Restarting logging process ====="));
        mCentralWidget->ensureCursorVisible();
        startWatcher();
    } else {
        KMessageBox::sorry(this, i18n("The watchgnupg logging process is not running.\nThis log window is unable to display any useful information."));
    }
}

void KWatchGnuPGMainWindow::slotReadStdout()
{
    if (!mWatcher) {
        return;
    }
    while (mWatcher->canReadLine()) {
        QString str = QString::fromUtf8(mWatcher->readLine());
        if (str.endsWith(QLatin1Char('\n'))) {
            str.chop(1);
        }
        if (str.endsWith(QLatin1Char('\r'))) {
            str.chop(1);
        }
        mCentralWidget->append(str);
        mCentralWidget->ensureCursorVisible();
        if (!isVisible()) {
            // Change tray icon to show something happened
            // PENDING(steffen)
            mSysTray->setAttention(true);
        }
    }
}

void KWatchGnuPGMainWindow::show()
{
    mSysTray->setAttention(false);
    KMainWindow::show();
}

void KWatchGnuPGMainWindow::slotSaveAs()
{
    const QString filename = QFileDialog::getSaveFileName(this, i18n("Save Log to File"));
    if (filename.isEmpty()) {
        return;
    }
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream(&file) << mCentralWidget->document()->toRawText();
    } else
        KMessageBox::information(this, i18n("Could not save file %1: %2",
                                            filename, file.errorString()));
}

void KWatchGnuPGMainWindow::slotQuit()
{
    disconnect(mWatcher, SIGNAL(finished(int,QProcess::ExitStatus)),
               this, SLOT(slotWatcherExited(int,QProcess::ExitStatus)));
    mWatcher->kill();
    qApp->quit();
}

void KWatchGnuPGMainWindow::slotConfigure()
{
    if (!mConfig) {
        mConfig = new KWatchGnuPGConfig(this);
        mConfig->setObjectName(QStringLiteral("config dialog"));
        connect(mConfig, &KWatchGnuPGConfig::reconfigure,
                this, &KWatchGnuPGMainWindow::slotReadConfig);
    }
    mConfig->loadConfig();
    mConfig->exec();
}

void KWatchGnuPGMainWindow::slotReadConfig()
{
    const KConfigGroup config(KSharedConfig::openConfig(), "LogWindow");
    const int maxLogLen = config.readEntry("MaxLogLen", 10000);
    mCentralWidget->document()->setMaximumBlockCount(maxLogLen < 1 ? -1 : maxLogLen);
    setGnuPGConfig();
    startWatcher();
}

bool KWatchGnuPGMainWindow::queryClose()
{
    if (!qApp->isSavingSession()) {
        hide();
        return false;
    }
    return KMainWindow::queryClose();
}

