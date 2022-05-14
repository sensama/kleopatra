/*  commands/setpivcardapplicationadministrationkeycommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "cardcommand.h"

namespace GpgME
{
class Error;
}

namespace Kleo
{
namespace Commands
{

class SetPIVCardApplicationAdministrationKeyCommand : public CardCommand
{
    Q_OBJECT
public:
    explicit SetPIVCardApplicationAdministrationKeyCommand(const std::string &serialNumber, QWidget *parent);
    ~SetPIVCardApplicationAdministrationKeyCommand() override;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotResult(GpgME::Error))
};

} // namespace Commands
} // namespace Kleo

