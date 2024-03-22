/* -*- mode: c++; c-basic-offset:4 -*-
    systemtrayicon.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "systrayicon.h"

#ifndef QT_NO_SYSTEMTRAYICON

#include "kleopatraapplication.h"
#include "mainwindow.h"

#include <smartcard/readerstatus.h>

#include <utils/clipboardmenu.h>

#include <commands/decryptverifyclipboardcommand.h>
#include <commands/encryptclipboardcommand.h>
#include <commands/importcertificatefromclipboardcommand.h>
#include <commands/learncardkeyscommand.h>
#include <commands/setinitialpincommand.h>
#include <commands/signclipboardcommand.h>

#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KActionMenu>
#include <KLocalizedString>
#include <QEventLoopLocker>
#include <QIcon>

#include <QAction>
#include <QApplication>
#include <QMenu>
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
        openCertificateManagerAction.setEnabled(!q->mainWindow() || !q->mainWindow()->isVisible());
        setInitialPinAction.setEnabled(!firstCardWithNullPin.empty());
        learnCertificatesAction.setEnabled(anyCardCanLearnKeys);

        q->setAttentionWanted((!firstCardWithNullPin.empty() || anyCardCanLearnKeys) && !q->attentionWindow());
    }

    void slotSetInitialPin()
    {
        if (!firstCardWithNullPin.empty()) {
            auto cmd = new SetInitialPinCommand(firstCardWithNullPin);
            q->setAttentionWindow(cmd->dialog());
            startCommand(cmd);
        }
    }

    void slotLearnCertificates()
    {
        auto cmd = new LearnCardKeysCommand(GpgME::CMS);
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
    std::string firstCardWithNullPin;
    bool anyCardCanLearnKeys = false;
    bool learningInProgress = false;

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
};

SysTrayIcon::Private::Private(SysTrayIcon *qq)
    : q(qq)
    , menu()
    , openCertificateManagerAction(i18nc("@action:inmenu", "&Open Certificate Manager..."), q)
    , configureAction(QIcon::fromTheme(QStringLiteral("configure")),
                      xi18nc("@action:inmenu", "&Configure <application>%1</application>...", KAboutData::applicationData().displayName()),
                      q)
    , aboutAction(QIcon::fromTheme(QStringLiteral("kleopatra")),
                  xi18nc("@action:inmenu", "&About <application>%1</application>...", KAboutData::applicationData().displayName()),
                  q)
    , quitAction(QIcon::fromTheme(QStringLiteral("application-exit")),
                 xi18nc("@action:inmenu", "&Shutdown <application>%1</application>", KAboutData::applicationData().displayName()),
                 q)
    , clipboardMenu(q)
    , cardMenu(i18nc("@title:menu", "SmartCard"))
    , updateCardStatusAction(i18nc("@action:inmenu", "Update Card Status"), q)
    , setInitialPinAction(i18nc("@action:inmenu", "Set NetKey v3 Initial PIN..."), q)
    , learnCertificatesAction(i18nc("@action:inmenu", "Learn NetKey v3 Card Certificates"), q)
    , aboutDialog()
{
    q->setNormalIcon(QIcon::fromTheme(QStringLiteral("kleopatra")));
    q->setAttentionIcon(QIcon::fromTheme(QStringLiteral("auth-sim-locked")));

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

SysTrayIcon::Private::~Private()
{
}

SysTrayIcon::SysTrayIcon(QObject *p)
    : SystemTrayIcon(p)
    , d(new Private(this))
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
            return; // ignore clicks while an attention window is open.
        }
    if (!d->firstCardWithNullPin.empty()) {
        d->slotSetInitialPin();
    } else if (d->anyCardCanLearnKeys) {
        d->slotLearnCertificates();
    } else {
        // Toggle visibility of MainWindow
        KleopatraApplication::instance()->toggleMainWindowVisibility();
    }
}

void SysTrayIcon::setFirstCardWithNullPin(const std::string &serialNumber)
{
    if (d->firstCardWithNullPin == serialNumber) {
        return;
    }
    d->firstCardWithNullPin = serialNumber;
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
