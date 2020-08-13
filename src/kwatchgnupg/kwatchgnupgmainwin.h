/*
    main.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2001, 2002, 2004 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWATCHGNUPGMAINWIN_H
#define KWATCHGNUPGMAINWIN_H

#include <kxmlguiwindow.h>
#include <QProcess>

class KWatchGnuPGTray;
class KWatchGnuPGConfig;
class KProcess;
class QTextEdit;

class KWatchGnuPGMainWindow : public KXmlGuiWindow
{
    Q_OBJECT
public:
    explicit KWatchGnuPGMainWindow(QWidget *parent = nullptr);
    ~KWatchGnuPGMainWindow();

private Q_SLOTS:
    void slotWatcherExited(int, QProcess::ExitStatus);
    void slotReadStdout();

    void slotSaveAs();
    void slotQuit();
    void slotClear();

    void slotConfigure();
    void slotConfigureToolbars();
    void configureShortcuts();
    void slotReadConfig();

public Q_SLOTS:
    /* reimp */ void show();

protected:
    bool queryClose() override;

private:
    void createActions();
    void startWatcher();
    void setGnuPGConfig();

    KProcess *mWatcher;

    QTextEdit *mCentralWidget;
    KWatchGnuPGTray *mSysTray;
    KWatchGnuPGConfig *mConfig;
};

#endif /* KWATCHGNUPGMAINWIN_H */

