// This file is part of Kleopatra, the KDE keymanager
// SPDX-FileCopyrightText: 2023 g10 Code GmbH
// SPDX-FileContributor: Carl Schwan <carl.schwan@gnupg.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "command.h"

namespace Kleo
{
namespace Commands
{

class ViewEmailFilesCommand : public Command
{
    Q_OBJECT
public:
    ViewEmailFilesCommand(const QStringList &files, KeyListController *parent);
    ~ViewEmailFilesCommand() override;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
};

} // end namespace Commands
} // end namespace Kleo
