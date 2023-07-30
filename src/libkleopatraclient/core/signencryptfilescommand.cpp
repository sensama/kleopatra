/* -*- mode: c++; c-basic-offset:4 -*-
    core/signencryptfilescommand.cpp

    This file is part of KleopatraClient, the Kleopatra interface library
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "signencryptfilescommand.h"

using namespace KleopatraClientCopy;

SignEncryptFilesCommand::SignEncryptFilesCommand(QObject *p)
    : Command(p)
{
    setCommand("SIGN_ENCRYPT_FILES");
    setOption("nohup");
}

SignEncryptFilesCommand::~SignEncryptFilesCommand()
{
}
