/* -*- mode: c++; c-basic-offset:4 -*-
    core/signencryptfilescommand.h

    This file is part of KleopatraClient, the Kleopatra interface library
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <libkleopatraclient/core/command.h>

namespace KleopatraClientCopy
{

class KLEOPATRACLIENTCORE_EXPORT SignEncryptFilesCommand : public Command
{
    Q_OBJECT
public:
    explicit SignEncryptFilesCommand(QObject *parent = nullptr);
    ~SignEncryptFilesCommand() override;

    // Inputs

    using Command::setFilePaths;
    using Command::filePaths;

    // No Outputs
};

}

