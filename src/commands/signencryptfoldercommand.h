/* -*- mode: c++; c-basic-offset:4 -*-
    commands/signencryptfoldercommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMMANDS_SIGNENCRYPTFOLDERCOMMAND_H__
#define __KLEOPATRA_COMMMANDS_SIGNENCRYPTFOLDERCOMMAND_H__

#include "signencryptfilescommand.h"

#include <QStringList>

namespace Kleo
{
namespace Commands
{

class SignEncryptFolderCommand : public SignEncryptFilesCommand
{
    Q_OBJECT
public:
    explicit SignEncryptFolderCommand(QAbstractItemView *view, KeyListController *parent);
    explicit SignEncryptFolderCommand(KeyListController *parent);

protected:
    QStringList selectFiles() const override;
};

}
}

#endif // __KLEOPATRA_COMMMANDS_SIGNENCRYPTFOLDERCOMMAND_H__
