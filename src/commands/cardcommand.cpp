/*  commands/cardcommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cardcommand.h"
#include "cardcommand_p.h"

#include <QWidget>

using namespace Kleo;

CardCommand::Private::Private(CardCommand *qq, const std::string &serialNumber, QWidget *parent)
    : q(qq),
      autoDelete(true),
      serialNumber_(serialNumber),
      parentWidget_(parent)
{
}

CardCommand::Private::~Private()
{
}

CardCommand::CardCommand(const std::string &serialNumber, QWidget *parent)
    : QObject(parent), d(new Private(this, serialNumber, parent))
{
}

CardCommand::CardCommand(Private *pp)
    : QObject(pp->parentWidget_), d(pp)
{
}

CardCommand::~CardCommand()
{
}

void CardCommand::setAutoDelete(bool on)
{
    d->autoDelete = on;
}

bool CardCommand::autoDelete() const
{
    return d->autoDelete;
}

void CardCommand::start()
{
    doStart();
}

void CardCommand::cancel()
{
    doCancel();
    Q_EMIT canceled();
}

void CardCommand::doCancel()
{
}
