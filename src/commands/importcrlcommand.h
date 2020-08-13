/* -*- mode: c++; c-basic-offset:4 -*-
    commands/importcrlcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMMANDS_IMPORTCRLCOMMAND_H__
#define __KLEOPATRA_COMMMANDS_IMPORTCRLCOMMAND_H__

#include <commands/command.h>

class QStringList;

namespace Kleo
{
namespace Commands
{

class ImportCrlCommand : public Command
{
    Q_OBJECT
public:
    explicit ImportCrlCommand(QAbstractItemView *view, KeyListController *parent);
    explicit ImportCrlCommand(KeyListController *parent);
    explicit ImportCrlCommand(const QStringList &files, QAbstractItemView *view, KeyListController *parent);
    explicit ImportCrlCommand(const QStringList &files, KeyListController *parent);
    ~ImportCrlCommand() override;

    void setFiles(const QStringList &files);

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotProcessFinished(int, QProcess::ExitStatus))
    Q_PRIVATE_SLOT(d_func(), void slotProcessReadyReadStandardError())
};

}
}

#endif // __KLEOPATRA_COMMMANDS_IMPORTCRLCOMMAND_H__
