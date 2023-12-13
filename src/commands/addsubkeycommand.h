/*
    SPDX-FileCopyrightText: 2023 g10 Code GmbH
    SPDX-FileContributor: Tobias Fella <tobias.fella@gnupg.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <commands/command.h>

namespace GpgME
{
class Subkey;
}

namespace Kleo
{
namespace Commands
{

class AddSubkeyCommand : public Command
{
    Q_OBJECT
public:
    explicit AddSubkeyCommand(const GpgME::Key &key);
    ~AddSubkeyCommand() override;

private:
    void doStart() override;
    void doCancel() override;

    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
};

}
}
