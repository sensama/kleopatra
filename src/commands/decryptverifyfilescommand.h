/* -*- mode: c++; c-basic-offset:4 -*-
    commands/decryptverifyfilescommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMMANDS_DECRYPTVERIFYFILESCOMMAND_H__
#define __KLEOPATRA_COMMMANDS_DECRYPTVERIFYFILESCOMMAND_H__

#include "commands/command.h"

#include "utils/types.h"

class QStringList;

namespace Kleo
{
namespace Commands
{

class DecryptVerifyFilesCommand : public Command
{
    Q_OBJECT
public:
    explicit DecryptVerifyFilesCommand(QAbstractItemView *view, KeyListController *parent);
    explicit DecryptVerifyFilesCommand(KeyListController *parent);
    explicit DecryptVerifyFilesCommand(const QStringList &files, QAbstractItemView *view, KeyListController *parent);
    explicit DecryptVerifyFilesCommand(const QStringList &files, KeyListController *parent, bool forceManualMode = false);
    ~DecryptVerifyFilesCommand() override;

    void setFiles(const QStringList &files);

    void setOperation(DecryptVerifyOperation operation);
    DecryptVerifyOperation operation() const;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotControllerDone())
    Q_PRIVATE_SLOT(d_func(), void slotControllerError(int, QString))
};

}
}

#endif // __KLEOPATRA_COMMMANDS_DECRYPTVERIFYFILESCOMMAND_H__
