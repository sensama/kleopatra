/* -*- mode: c++; c-basic-offset:4 -*-
    uiserver/signcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEO_UISERVER_SIGNCOMMAND_H__
#define __KLEO_UISERVER_SIGNCOMMAND_H__

#include "assuancommand.h"

#include <utils/pimpl_ptr.h>

namespace Kleo
{

class SignCommand : public Kleo::AssuanCommandMixin<SignCommand>
{
public:
    SignCommand();
    ~SignCommand() override;

private:
    int doStart() override;
    void doCanceled() override;
public:
    static const char *staticName()
    {
        return "SIGN";
    }

    class Private;
private:
    kdtools::pimpl_ptr<Private> d;
};

}

#endif /*__KLEO_UISERVER_SIGNCOMMAND_H__*/
