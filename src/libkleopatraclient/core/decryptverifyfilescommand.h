/* -*- mode: c++; c-basic-offset:4 -*-
    core/decryptverifyfilescommand.h

    This file is part of KleopatraClient, the Kleopatra interface library
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <libkleopatraclient/core/command.h>

namespace KleopatraClientCopy
{

class KLEOPATRACLIENTCORE_EXPORT DecryptVerifyFilesCommand : public Command
{
    Q_OBJECT
public:
    explicit DecryptVerifyFilesCommand(QObject *parent = nullptr);
    ~DecryptVerifyFilesCommand() override;

    // Inputs

    using Command::filePaths;
    using Command::setFilePaths;

    // No Outputs
};

}
