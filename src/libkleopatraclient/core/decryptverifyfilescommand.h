/* -*- mode: c++; c-basic-offset:4 -*-
    core/decryptverifyfilescommand.h

    This file is part of KleopatraClient, the Kleopatra interface library
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef __LIBKLEOPATRACLIENT_CORE_DECRYPTVERIFYFILESCOMMAND_H__
#define __LIBKLEOPATRACLIENT_CORE_DECRYPTVERIFYFILESCOMMAND_H__

#include <libkleopatraclient/core/command.h>

namespace KleopatraClientCopy
{

class KLEOPATRACLIENTCORE_EXPORT DecryptVerifyFilesCommand : public Command
{
    Q_OBJECT
public:
    explicit DecryptVerifyFilesCommand(QObject *parent = nullptr);
    ~DecryptVerifyFilesCommand();

    // Inputs

    using Command::setFilePaths;
    using Command::filePaths;

    // No Outputs
};

}

#endif /* __LIBKLEOPATRACLIENT_CORE_DECRYPTVERIFYFILESCOMMAND_H__ */
