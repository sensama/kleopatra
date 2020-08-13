/* -*- mode: c++; c-basic-offset:4 -*-
    uiserver/decryptverifycommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UISERVER_DECRYPTVERIFYCOMMANDEMAILBASE_H__
#define __KLEOPATRA_UISERVER_DECRYPTVERIFYCOMMANDEMAILBASE_H__

#include "assuancommand.h"

#include <utils/pimpl_ptr.h>
#include <utils/types.h>

namespace Kleo
{

class DecryptVerifyCommandEMailBase : public AssuanCommandMixin<DecryptVerifyCommandEMailBase>
{
public:
    explicit DecryptVerifyCommandEMailBase();
    ~DecryptVerifyCommandEMailBase() override;

private:
    virtual DecryptVerifyOperation operation() const = 0;
    virtual Mode mode() const
    {
        return EMail;
    }

private:
    int doStart() override;
    void doCanceled() override;
public:
    static const char *staticName()
    {
        return "";
    }

    class Private;
private:
    kdtools::pimpl_ptr<Private> d;
};

class DecryptVerifyCommand : public AssuanCommandMixin<DecryptVerifyCommand, DecryptVerifyCommandEMailBase>
{
public:

private:
    DecryptVerifyOperation operation() const override
    {
        return DecryptVerify;
    }

public:
    static const char *staticName()
    {
        return "DECRYPT_VERIFY";
    }
};
}

#endif // __KLEOPATRA_UISERVER_DECRYPTCOMMAND_H__
