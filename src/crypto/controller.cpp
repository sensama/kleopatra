/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/controller.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config-kleopatra.h>

#include "controller.h"


using namespace Kleo;
using namespace Kleo::Crypto;

class Controller::Private
{
    friend class ::Kleo::Crypto::Controller;
    Controller *const q;
public:
    explicit Private(Controller *qq)
        : q(qq),
          lastError(0),
          lastErrorString()
    {

    }

private:
    int lastError;
    QString lastErrorString;
};

Controller::Controller(QObject *parent)
    : QObject(parent), ExecutionContextUser(), d(new Private(this))
{

}

Controller::Controller(const std::shared_ptr<const ExecutionContext> &ctx, QObject *parent)
    : QObject(parent), ExecutionContextUser(ctx), d(new Private(this))
{

}

Controller::~Controller() {}

void Controller::taskDone(const std::shared_ptr<const Task::Result> &result)
{
    const Task *task = qobject_cast<const Task *>(sender());
    Q_ASSERT(task);
    doTaskDone(task, result);
}

void Controller::doTaskDone(const Task *, const std::shared_ptr<const Task::Result> &) {}

void Controller::connectTask(const std::shared_ptr<Task> &task)
{
    Q_ASSERT(task);
    connect(task.get(), &Task::result, this, &Controller::taskDone);
}

void Controller::setLastError(int err, const QString &msg)
{
    d->lastError = err;
    d->lastErrorString = msg;
}

void Controller::emitDoneOrError()
{
    if (d->lastError) {
        Q_EMIT error(d->lastError, d->lastErrorString);
        d->lastError = 0;
        d->lastErrorString = QString();
    } else {
        done();
    }
}

