/*
    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2024 g10 Code GmbH
    SPDX-FileContributor: Tobias Fella <tobias.fella@gnupg.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "command.h"

namespace Kleo
{
class CreateGroupCommand : public Command
{
    Q_OBJECT
public:
    explicit CreateGroupCommand(QAbstractItemView *view, KeyListController *parent);
    ~CreateGroupCommand() override;

    /* reimp */ static Restrictions restrictions()
    {
        return NeedSelection;
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
