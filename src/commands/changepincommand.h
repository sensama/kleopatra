/*  commands/changepincommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef __KLEOPATRA_COMMMANDS_CHANGEPINCOMMAND_H__
#define __KLEOPATRA_COMMMANDS_CHANGEPINCOMMAND_H__

#include "cardcommand.h"

namespace GpgME
{
class Error;
}

namespace Kleo
{
namespace Commands
{

class ChangePinCommand : public CardCommand
{
    Q_OBJECT
public:
    explicit ChangePinCommand(const std::string &serialNumber, const std::string &appName, QWidget *parent);
    ~ChangePinCommand() override;

    void setKeyRef(const std::string &keyRef);

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

#endif // __KLEOPATRA_COMMMANDS_CHANGEPINCOMMAND_H__

