/* -*- mode: c++; c-basic-offset:4 -*-
    systemtrayicon.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "systrayicon.h"

#ifndef QT_NO_SYSTEMTRAYICON

#include "mainwindow.h"
#include "kleopatraapplication.h"

#include <smartcard/readerstatus.h>

#include <utils/clipboardmenu.h>

#include <commands/importcertificatefromclipboardcommand.h>
#include <commands/encryptclipboardcommand.h>
#include <commands/signclipboardcommand.h>
#include <commands/decryptverifyclipboardcommand.h>
#include <commands/setinitialpincommand.h>
#include <commands/learncardkeyscommand.h>

#include <QIcon>
#include <KLocalizedString>
#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KActionMenu>
#include <QEventLoopLocker>

#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QPointer>
#include <QSignalBlocker>


using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::SmartCard;

class SysTrayIcon::Private
{
    friend class ::SysTrayIcon;
    SysTrayIcon *const q;
public:
    explicit Private(SysTrayIcon *qq);
    ~Private();

private:
    void slotAbout()
    {
        if (!aboutDialog) {
            aboutDialog = new KAboutApplicationDialog(KAboutData::applicationData());
            aboutDialog->setAttribute(Qt::WA_DeleteOnClose);
        }

        if (aboutDialog->isVisible()) {
            aboutDialog->raise();
        } else {
            aboutDialog->show();
        }
    }

    void enableDisableActions()
    {
        //work around a Qt bug (seen with Qt 4.4.0, Windows): QClipBoard->mimeData() triggers QClipboard::changed(),
        //triggering slotEnableDisableActions again
        const QSignalBlocker blocker(QApplication::clipboard());
        openCertificateManagerAction.setEnabled(!q->mainWindow() || !q->mainWindow()->isVisible());
        setInitialPinAction.setEnabled(anyCardHasNullPin);
        learnCertificatesAction.setEnabled(anyCardCanLearnKeys);

        q->setAttentionWanted((anyCardHasNullPin || anyCardCanLearnKeys) && !q->attentionWindow());
    }

    void slotSetInitialPin()
    {
        SetInitialPinCommand *cmd = new SetInitialPinCommand;
        q->setAttentionWindow(cmd->dialog());
        startCommand(cmd);
    }

    void slotLearnCertificates()
    {
        LearnCardKeysCommand *cmd = new LearnCardKeysCommand(GpgME::CMS);
        q->setAttentionWindow(cmd->dialog());
        startCommand(cmd);
    }
    void startCommand(Command *cmd)
    {
        Q_ASSERT(cmd);
        cmd->setParent(q->mainWindow());
        cmd->start();
    }

private:
    bool anyCardHasNullPin;
    bool anyCardCanLearnKeys;
    bool learningInProgress;

    QMenu menu;
    QAction openCertificateManagerAction;
    QAction configureAction;
    QAction aboutAction;
    QAction quitAction;

    ClipboardMenu clipboardMenu;

    QMenu cardMenu;
    QAction updateCardStatusAction;
    QAction setInitialPinAction;
    QAction learnCertificatesAction;

    QPointer<KAboutApplicationDialog> aboutDialog;
    QEventLoopLocker eventLoopLocker;
};

SysTrayIcon::Private::Private(SysTrayIcon *qq)
    : q(qq),
      anyCardHasNullPin(false),
      anyCardCanLearnKeys(false),
      menu(),
      openCertificateManagerAction(i18n("&Open Certificate Manager..."), q),
      configureAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("&Configure %1...", KAboutData::applicationData().componentName()), q),
      aboutAction(QIcon::fromTheme(QStringLiteral("kleopatra")), i18n("&About %1...", KAboutData::applicationData().componentName()), q),
      quitAction(QIcon::fromTheme(QStringLiteral("application-exit")),i18n("&Shutdown Kleopatra"), q),
      clipboardMenu(q),
      cardMenu(i18n("SmartCard")),
      updateCardStatusAction(i18n("Update Card Status"), q),
      setInitialPinAction(i18n("Set NetKey v3 Initial PIN..."), q),
      learnCertificatesAction(i18n("Learn NetKey v3 Card Certificates"), q),
      aboutDialog()
{
    q->setNormalIcon(QIcon::fromTheme(QStringLiteral("kleopatra")));
    q->setAttentionIcon(QIcon::fromTheme(QStringLiteral("secure-card")));

    KDAB_SET_OBJECT_NAME(menu);
    KDAB_SET_OBJECT_NAME(openCertificateManagerAction);
    KDAB_SET_OBJECT_NAME(configureAction);
    KDAB_SET_OBJECT_NAME(aboutAction);
    KDAB_SET_OBJECT_NAME(quitAction);
    KDAB_SET_OBJECT_NAME(clipboardMenu);
    KDAB_SET_OBJECT_NAME(cardMenu);
    KDAB_SET_OBJECT_NAME(setInitialPinAction);
    KDAB_SET_OBJECT_NAME(learnCertificatesAction);

    connect(&openCertificateManagerAction, SIGNAL(triggered()), qApp, SLOT(openOrRaiseMainWindow()));
    connect(&configureAction, SIGNAL(triggered()), qApp, SLOT(openOrRaiseConfigDialog()));
    connect(&aboutAction, SIGNAL(triggered()), q, SLOT(slotAbout()));
    connect(&quitAction, &QAction::triggered, QCoreApplication::instance(), &QCoreApplication::quit);
    connect(&updateCardStatusAction, &QAction::triggered, ReaderStatus::instance(), &ReaderStatus::updateStatus);
    connect(&setInitialPinAction, SIGNAL(triggered()), q, SLOT(slotSetInitialPin()));
    connect(&learnCertificatesAction, SIGNAL(triggered()), q, SLOT(slotLearnCertificates()));

    connect(QApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)),
            q, SLOT(slotEnableDisableActions()));

    menu.addAction(&openCertificateManagerAction);
    menu.addAction(&configureAction);
    menu.addAction(&aboutAction);
    menu.addSeparator();
    menu.addMenu(clipboardMenu.clipboardMenu()->menu());
    menu.addSeparator();
    menu.addMenu(&cardMenu);
    cardMenu.addAction(&updateCardStatusAction);
    cardMenu.addAction(&setInitialPinAction);
    cardMenu.addAction(&learnCertificatesAction);
    menu.addSeparator();
    menu.addAction(&quitAction);

    q->setContextMenu(&menu);
    clipboardMenu.setMainWindow(q->mainWindow());
}

SysTrayIcon::Private::~Private() {}

SysTrayIcon::SysTrayIcon(QObject *p)
    : SystemTrayIcon(p), d(new Private(this))
{
    slotEnableDisableActions();
}

SysTrayIcon::~SysTrayIcon()
{
}

MainWindow *SysTrayIcon::mainWindow() const
{
    return static_cast<MainWindow *>(SystemTrayIcon::mainWindow());
}

QDialog *SysTrayIcon::attentionWindow() const
{
    return static_cast<QDialog *>(SystemTrayIcon::attentionWindow());
}

void SysTrayIcon::doActivated()
{
    if (const QWidget *const aw = attentionWindow())
        if (aw->isVisible()) {
            return;    // ignore clicks while an attention window is open.
        }
    if (d->anyCardHasNullPin) {
        d->slotSetInitialPin();
    } else if (d->anyCardCanLearnKeys) {
        d->slotLearnCertificates();
    } else {
        // Toggle visibility of MainWindow
        KleopatraApplication::instance()->toggleMainWindowVisibility();
    }
}

void SysTrayIcon::setAnyCardHasNullPin(bool on)
{
    if (d->anyCardHasNullPin == on) {
        return;
    }
    d->anyCardHasNullPin = on;
    slotEnableDisableActions();
}

void SysTrayIcon::setAnyCardCanLearnKeys(bool on)
{
    if (d->anyCardCanLearnKeys == on || d->learningInProgress) {
        return;
    }
    d->anyCardCanLearnKeys = on;
    slotEnableDisableActions();
}

void SysTrayIcon::slotEnableDisableActions()
{
    d->enableDisableActions();
}

/* We need this as the readerstatus might update even
 * while the loading is in progress. */
void SysTrayIcon::setLearningInProgress(bool value)
{
    if (value) {
        setAnyCardCanLearnKeys(false);
    }
    d->learningInProgress = value;
}

#include "moc_systrayicon.cpp"

#endif // QT_NO_SYSTEMTRAYICON
