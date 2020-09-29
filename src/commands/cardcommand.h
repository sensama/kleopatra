/*  commands/cardcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_CARDCOMMAND_H__
#define __KLEOPATRA_COMMANDS_CARDCOMMAND_H__

#include "command.h"

namespace Kleo
{

class CardCommand : public Command
{
    Q_OBJECT
public:
    explicit CardCommand(const std::string &serialNumber, QWidget *parent);
    ~CardCommand() override;

protected:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;

protected:
    explicit CardCommand(Private *pp);
};

} // namespace Kleo

#endif /* __KLEOPATRA_COMMANDS_CARDCOMMAND_H__ */
