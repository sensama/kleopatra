/*  commands/cardcommand_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_CARDCOMMAND_P_H__
#define __KLEOPATRA_COMMANDS_CARDCOMMAND_P_H__

#include "cardcommand.h"
#include "command_p.h"

class Kleo::CardCommand::Private : public Command::Private
{
    friend class ::Kleo::CardCommand;
    Kleo::CardCommand *q_func() const
    {
        return static_cast<Kleo::CardCommand *>(q);
    }
public:
    explicit Private(CardCommand *qq, const std::string &serialNumber, QWidget *parent);
    ~Private();

    std::string serialNumber() const
    {
        return serialNumber_;
    }

protected:
    void setSerialNumber(const std::string &serialNumber)
    {
        serialNumber_ = serialNumber;
    }

private:
    std::string serialNumber_;
};

#endif /* __KLEOPATRA_COMMANDS_CARDCOMMAND_P_H__ */
