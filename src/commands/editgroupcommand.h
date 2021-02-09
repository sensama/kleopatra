/* -*- mode: c++; c-basic-offset:4 -*-
    commands/editgroupcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_EDITGROUPCOMMAND_H__
#define __KLEOPATRA_COMMANDS_EDITGROUPCOMMAND_H__

#include "command.h"

namespace Kleo
{
class KeyGroup;

namespace Commands
{

class EditGroupCommand : public Command
{
    Q_OBJECT
public:
    explicit EditGroupCommand(const KeyGroup &group, QWidget *parent = nullptr);
    ~EditGroupCommand() override;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
};

} // namespace Commands
} // namespace Kleo

#endif // __KLEOPATRA_COMMANDS_EDITGROUPCOMMAND_H__
