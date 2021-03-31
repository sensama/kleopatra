/* -*- mode: c++; c-basic-offset:4 -*-
    uiserver/prepsigncommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "assuancommand.h"

#include <utils/pimpl_ptr.h>

namespace Kleo
{

class PrepSignCommand : public Kleo::AssuanCommandMixin<PrepSignCommand>
{
public:
    PrepSignCommand();
    virtual ~PrepSignCommand();
private:
    int doStart() override;
    void doCanceled() override;
public:
    static const char *staticName()
    {
        return "PREP_SIGN";
    }

    class Private;
private:
    kdtools::pimpl_ptr<Private> d;
};

}

