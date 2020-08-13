/* -*- mode: c++; c-basic-offset:4 -*-
    core/decryptverifyfilescommand.cpp

    This file is part of KleopatraClient, the Kleopatra interface library
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "decryptverifyfilescommand.h"

using namespace KleopatraClientCopy;

DecryptVerifyFilesCommand::DecryptVerifyFilesCommand(QObject *p)
    : Command(p)
{
    setCommand("DECRYPT_VERIFY_FILES");
    setOption("nohup");
}

DecryptVerifyFilesCommand::~DecryptVerifyFilesCommand() {}

