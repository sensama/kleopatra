/* -*- mode: c++; c-basic-offset:4 -*-
    uiserver/decryptemailcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UISERVER_DECRYPTVERIFYFILESCOMMAND_H__
#define __KLEOPATRA_UISERVER_DECRYPTVERIFYFILESCOMMAND_H__

#include <uiserver/decryptverifycommandfilesbase.h>

namespace Kleo
{

class DecryptVerifyFilesCommand : public AssuanCommandMixin<DecryptVerifyFilesCommand, DecryptVerifyCommandFilesBase>
{
public:
    //DecryptVerifyFilesCommand();
    //~DecryptVerifyFilesCommand();

private:
    DecryptVerifyOperation operation() const override
    {
        return DecryptVerify;
    }
public:
    static const char *staticName()
    {
        return "DECRYPT_VERIFY_FILES";
    }
};

}

#endif // __KLEOPATRA_UISERVER_DECRYPTVERIFYFILESCOMMAND_H__
