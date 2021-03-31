/* -*- mode: c++; c-basic-offset:4 -*-
    controllers/keylistcontroller.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

#include <commands/command.h>

#include <utils/pimpl_ptr.h>

#include <vector>

class QAbstractItemView;
class QAction;
class QPoint;
class QItemSelectionModel;
class KActionCollection;

namespace Kleo
{

class AbstractKeyListModel;
class Command;
class TabWidget;

class KeyListController : public QObject
{
    Q_OBJECT
public:
    explicit KeyListController(QObject *parent = nullptr);
    ~KeyListController();

    std::vector<QAbstractItemView *> views() const;

    void setFlatModel(AbstractKeyListModel *model);
    AbstractKeyListModel *flatModel() const;

    void setHierarchicalModel(AbstractKeyListModel *model);
    AbstractKeyListModel *hierarchicalModel() const;

    void setParentWidget(QWidget *parent);
    QWidget *parentWidget() const;

    QAbstractItemView *currentView() const;

    void setTabWidget(TabWidget *tabs);
    TabWidget *tabWidget() const;

    void registerCommand(Command *cmd);

    void createActions(KActionCollection *collection);

    template <typename T_Command>
    void registerActionForCommand(QAction *action)
    {
        this->registerAction(action, T_Command::restrictions(), &KeyListController::template create<T_Command>);
    }

    void enableDisableActions(const QItemSelectionModel *sm) const;

    bool hasRunningCommands() const;
    bool shutdownWarningRequired() const;

private:
    void registerAction(QAction *action, Command::Restrictions restrictions, Command * (*create)(QAbstractItemView *, KeyListController *));

    template <typename T_Command>
    static Command *create(QAbstractItemView *v, KeyListController *c)
    {
        return new T_Command(v, c);
    }

public Q_SLOTS:
    void addView(QAbstractItemView *view);
    void removeView(QAbstractItemView *view);
    void setCurrentView(QAbstractItemView *view);

    void cancelCommands();
    void updateConfig();

Q_SIGNALS:
    void progress(int current, int total);
    void message(const QString &msg, int timeout = 0);

    void commandsExecuting(bool);

    void contextMenuRequested(QAbstractItemView *view, const QPoint &p);

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;

    Q_PRIVATE_SLOT(d, void slotDestroyed(QObject *))
    Q_PRIVATE_SLOT(d, void slotDoubleClicked(QModelIndex))
    Q_PRIVATE_SLOT(d, void slotActivated(QModelIndex))
    Q_PRIVATE_SLOT(d, void slotSelectionChanged(QItemSelection, QItemSelection))
    Q_PRIVATE_SLOT(d, void slotContextMenu(QPoint))
    Q_PRIVATE_SLOT(d, void slotCommandFinished())
    Q_PRIVATE_SLOT(d, void slotAddKey(GpgME::Key))
    Q_PRIVATE_SLOT(d, void slotAboutToRemoveKey(GpgME::Key))
    Q_PRIVATE_SLOT(d, void slotProgress(QString, int, int))
    Q_PRIVATE_SLOT(d, void slotActionTriggered())
    Q_PRIVATE_SLOT(d, void slotCurrentViewChanged(QAbstractItemView *))
};

}

