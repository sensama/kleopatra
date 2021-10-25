/*
  SPDX-FileCopyrightText: 2014-2021 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-only
*/

#pragma once

#include <QObject>
class KActionMenu;
class QAction;
class MainWindow;
namespace Kleo
{
class Command;
}
class ClipboardMenu : public QObject
{
    Q_OBJECT
public:
    explicit ClipboardMenu(QObject *parent = nullptr);
    ~ClipboardMenu() override;

    void setMainWindow(MainWindow *window);

    KActionMenu *clipboardMenu() const;

private Q_SLOTS:
    void slotImportClipboard();
    void slotEncryptClipboard();
    void slotOpenPGPSignClipboard();
    void slotSMIMESignClipboard();
    void slotDecryptVerifyClipboard();
    void slotEnableDisableActions();

private:
    void startCommand(Kleo::Command *cmd);

    KActionMenu *mClipboardMenu;
    QAction *mImportClipboardAction;
    QAction *mEncryptClipboardAction;
    QAction *mSmimeSignClipboardAction;
    QAction *mOpenPGPSignClipboardAction;
    QAction *mDecryptVerifyClipboardAction;
    MainWindow *mWindow;
};

