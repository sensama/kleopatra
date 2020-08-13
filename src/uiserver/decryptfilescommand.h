/* -*- mode: c++; c-basic-offset:4 -*-
    uiserver/decryptemailcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UISERVER_DECRYPTFILESCOMMAND_H__
#define __KLEOPATRA_UISERVER_DECRYPTFILESCOMMAND_H__

#include <uiserver/decryptverifycommandfilesbase.h>

namespace Kleo
{

class DecryptFilesCommand : public AssuanCommandMixin<DecryptFilesCommand, DecryptVerifyCommandFilesBase>
{
public:
    //DecryptFilesCommand();
    //~DecryptFilesCommand();

private:
    DecryptVerifyOperation operation() const override
    {
        return Decrypt;
    }
public:
    static const char *staticName()
    {
        return "DECRYPT_FILES";
    }
};

}

#endif // __KLEOPATRA_UISERVER_DECRYPTFILESCOMMAND_H__
