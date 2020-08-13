/* -*- mode: c++; c-basic-offset:4 -*-
    uiserver/decryptemailcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UISERVER_DECRYPTCOMMAND_H__
#define __KLEOPATRA_UISERVER_DECRYPTCOMMAND_H__

#include <uiserver/decryptverifycommandemailbase.h>

namespace Kleo
{

class DecryptCommand : public AssuanCommandMixin<DecryptCommand, DecryptVerifyCommandEMailBase>
{
public:
    //DecryptCommand();
    //~DecryptCommand();

private:
    DecryptVerifyOperation operation() const override
    {
        if (hasOption("no-verify")) {
            return Decrypt;
        } else {
            return DecryptVerify;
        }
    }
public:
    static const char *staticName()
    {
        return "DECRYPT";
    }
};

}

#endif // __KLEOPATRA_UISERVER_DECRYPTCOMMAND_H__
