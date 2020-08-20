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

CardCommand::Private::Private(CardCommand *qq, QWidget *parent)
    : q(qq),
      autoDelete(true),
      parentWidget_(parent)
{
}

CardCommand::Private::~Private()
{
}

CardCommand::CardCommand(QWidget *parent)
    : QObject(parent), d(new Private(this, parent))
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
