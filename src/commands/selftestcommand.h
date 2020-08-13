/* -*- mode: c++; c-basic-offset:4 -*-
    commands/selftestcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMMANDS_SELFTESTCOMMAND_H__
#define __KLEOPATRA_COMMMANDS_SELFTESTCOMMAND_H__

#include <commands/command.h>

namespace Kleo
{
namespace Commands
{

class SelfTestCommand : public Command
{
    Q_OBJECT
public:
    explicit SelfTestCommand(QAbstractItemView *view, KeyListController *parent);
    explicit SelfTestCommand(KeyListController *parent);
    ~SelfTestCommand() override;

    void setAutomaticMode(bool automatic);

    bool isCanceled() const;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotUpdateRequested())
    Q_PRIVATE_SLOT(d_func(), void slotDialogAccepted())
    Q_PRIVATE_SLOT(d_func(), void slotDialogRejected())
};

}
}

#endif // __KLEOPATRA_COMMMANDS_SELFTESTCOMMAND_H__
