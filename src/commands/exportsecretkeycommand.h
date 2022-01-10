/* -*- mode: c++; c-basic-offset:4 -*-
    commands/exportsecretkeycommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "command.h"

namespace Kleo
{
namespace Commands
{

class ExportSecretKeyCommand : public Command
{
    Q_OBJECT
public:
    explicit ExportSecretKeyCommand(QAbstractItemView *view, KeyListController *parent);
    explicit ExportSecretKeyCommand(const GpgME::Key &key);
    ~ExportSecretKeyCommand() override;

    /* reimp */ static Restrictions restrictions()
    {
        return OnlyOneKey | NeedSecretKey;
    }

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
};

}
}
