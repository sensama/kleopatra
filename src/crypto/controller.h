/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/controller.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_CRYPTO_CONTROLLER_H__
#define __KLEOPATRA_CRYPTO_CONTROLLER_H__

#include <QObject>

#include <crypto/task.h>

#include <utils/pimpl_ptr.h>
#include <utils/types.h>

#include <memory>

namespace Kleo
{
namespace Crypto
{

class Controller : public QObject, protected ExecutionContextUser
{
    Q_OBJECT
public:
    explicit Controller(QObject *parent = nullptr);
    explicit Controller(const std::shared_ptr<const ExecutionContext> &cmd, QObject *parent = nullptr);
    ~Controller();

    using ExecutionContextUser::setExecutionContext;

Q_SIGNALS:
    void progress(int current, int total, const QString &what);

protected:
    void emitDoneOrError();
    void setLastError(int err, const QString &details);
    void connectTask(const std::shared_ptr<Task> &task);

    virtual void doTaskDone(const Task *task, const std::shared_ptr<const Task::Result> &result);

protected Q_SLOTS:
    void taskDone(const std::shared_ptr<const Kleo::Crypto::Task::Result> &);

Q_SIGNALS:

#ifndef Q_MOC_RUN
# ifndef DOXYGEN_SHOULD_SKIP_THIS
private: // don't tell moc or doxygen, but those signals are in fact private
# endif
#endif
    void error(int err, const QString &details);
    void done();

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

}
}

#endif /* __KLEOPATRA_CRYPTO_CONTROLLER_H__ */
