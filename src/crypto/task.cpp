/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/task.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

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
#include "task.h"
#include "task_p.h"
#include "kleopatra_debug.h"

#include <Libkleo/Exception>

#include <utils/gnupg-helper.h>
#include <utils/auditlog.h>

#include <gpgme++/exception.h>

#include <gpg-error.h>

#include <KIconLoader>
#include <KLocalizedString>

#include <QString>

using namespace Kleo;
using namespace Kleo::Crypto;
using namespace GpgME;

namespace
{

class ErrorResult : public Task::Result
{
public:
    ErrorResult(int code, const QString &details)
        : Task::Result(), m_code(code), m_details(details) {}

    QString overview() const override
    {
        return makeOverview(m_details);
    }
    QString details() const override
    {
        return QString();
    }
    int errorCode() const override
    {
        return m_code;
    }
    QString errorString() const override
    {
        return m_details;
    }
    VisualCode code() const override
    {
        return NeutralError;
    }
    AuditLog auditLog() const override
    {
        return AuditLog();
    }
private:
    int m_code;
    QString m_details;
};
}

class Task::Private
{
    friend class ::Kleo::Crypto::Task;
    Task *const q;
public:
    explicit Private(Task *qq);

private:
    QString m_progressLabel;
    int m_progress;
    int m_totalProgress;
    bool m_asciiArmor;
    int m_id;
};

namespace
{
static int nextTaskId = 0;
}

Task::Private::Private(Task *qq)
    : q(qq), m_progressLabel(), m_progress(0), m_totalProgress(0), m_asciiArmor(false), m_id(nextTaskId++)
{

}

Task::Task(QObject *p)
    : QObject(p), d(new Private(this))
{

}

Task::~Task() {}

void Task::setAsciiArmor(bool armor)
{
    d->m_asciiArmor = armor;
}

bool Task::asciiArmor() const
{
    return d->m_asciiArmor;
}

std::shared_ptr<Task> Task::makeErrorTask(int code, const QString &details, const QString &label)
{
    const std::shared_ptr<SimpleTask> t(new SimpleTask(label));
    t->setResult(t->makeErrorResult(code, details));
    return t;
}

int Task::id() const
{
    return d->m_id;
}

int Task::currentProgress() const
{
    return d->m_progress;
}

int Task::totalProgress() const
{
    return d->m_totalProgress;
}

QString Task::tag() const
{
    return QString();
}

QString Task::progressLabel() const
{
    return d->m_progressLabel;
}

void Task::setProgress(const QString &label, int processed, int total)
{
    d->m_progress = processed;
    d->m_totalProgress = total;
    d->m_progressLabel = label;
    Q_EMIT progress(label, processed, total, QPrivateSignal());
}

void Task::start()
{
    try {
        doStart();
    } catch (const Kleo::Exception &e) {
        QMetaObject::invokeMethod(this, "emitError", Qt::QueuedConnection, Q_ARG(int, e.error().encodedError()), Q_ARG(QString, e.message()));
    } catch (const GpgME::Exception &e) {
        QMetaObject::invokeMethod(this, "emitError", Qt::QueuedConnection, Q_ARG(int, e.error().encodedError()), Q_ARG(QString, QString::fromLocal8Bit(e.what())));
    } catch (const std::exception &e) {
        QMetaObject::invokeMethod(this, "emitError", Qt::QueuedConnection, Q_ARG(int, makeGnuPGError(GPG_ERR_UNEXPECTED)), Q_ARG(QString, QString::fromLocal8Bit(e.what())));
    } catch (...) {
        QMetaObject::invokeMethod(this, "emitError", Qt::QueuedConnection, Q_ARG(int, makeGnuPGError(GPG_ERR_UNEXPECTED)), Q_ARG(QString, i18n("Unknown exception in Task::start()")));
    }
    Q_EMIT started(QPrivateSignal());
}

void Task::emitError(int errCode, const QString &details)
{
    emitResult(makeErrorResult(errCode, details));
}

void Task::emitResult(const std::shared_ptr<const Task::Result> &r)
{
    d->m_progress = d->m_totalProgress;
    Q_EMIT progress(progressLabel(), currentProgress(), totalProgress(), QPrivateSignal());
    Q_EMIT result(r, QPrivateSignal());
}

std::shared_ptr<Task::Result> Task::makeErrorResult(int errCode, const QString &details)
{
    return std::shared_ptr<Task::Result>(new ErrorResult(errCode, details));
}

class Task::Result::Private
{
public:
    Private() {}
};

Task::Result::Result() : d(new Private()) {}
Task::Result::~Result() {}

bool Task::Result::hasError() const
{
    return errorCode() != 0;
}

static QString image(const char *img)
{
    // ### escape?
    return KIconLoader::global()->iconPath(QLatin1String(img), KIconLoader::Small);
}

QString Task::Result::makeOverview(const QString &msg)
{
    return QLatin1String("<b>") + msg + QLatin1String("</b>");
}

QString Task::Result::iconPath(VisualCode code)
{
    switch (code) {
    case Danger:
        return image("dialog-error");
    case AllGood:
        return image("dialog-ok");
    case Warning:
        return image("dialog-warning");
    case NeutralError:
    case NeutralSuccess:
    default:
        return QString();

    }
}

QString Task::Result::icon() const
{
    return iconPath(code());
}

#include "moc_task_p.cpp"
