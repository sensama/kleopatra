/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/task.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_CRYPTO_TASK_H__
#define __KLEOPATRA_CRYPTO_TASK_H__

#include <QObject>
#include <QString>

#include <utils/pimpl_ptr.h>

#include <gpgme++/global.h>

#include <memory>
#include <QPointer>

namespace Kleo
{
class AuditLog;
}

namespace Kleo
{
namespace Crypto
{

class Task : public QObject
{
    Q_OBJECT
public:
    explicit Task(QObject *parent = nullptr);
    ~Task();

    class Result;

    void setAsciiArmor(bool armor);
    bool asciiArmor() const;

    virtual GpgME::Protocol protocol() const = 0;

    void start();

    virtual QString label() const = 0;

    virtual QString tag() const;

    QString progressLabel() const;
    int currentProgress() const;
    int totalProgress() const;

    int id() const;

    static std::shared_ptr<Task> makeErrorTask(int code, const QString &details, const QString &label);

public Q_SLOTS:
    virtual void cancel() = 0;

Q_SIGNALS:
    void progress(const QString &what, int processed, int total, QPrivateSignal);
    void result(const std::shared_ptr<const Kleo::Crypto::Task::Result> &, QPrivateSignal);
    void started(QPrivateSignal);

protected:
    std::shared_ptr<Result> makeErrorResult(int errCode, const QString &details);

    void emitResult(const std::shared_ptr<const Task::Result> &result);

protected Q_SLOTS:
    void setProgress(const QString &msg, int processed, int total);

private Q_SLOTS:
    void emitError(int errCode, const QString &details);

private:
    virtual void doStart() = 0;
    virtual unsigned long long inputSize() const = 0;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

class Task::Result
{
    const QString m_nonce;
public:
    Result();
    virtual ~Result();

    const QString &nonce() const
    {
        return m_nonce;
    }

    bool hasError() const;

    enum VisualCode {
        AllGood,
        Warning,
        Danger,
        NeutralSuccess,
        NeutralError
    };

    virtual QString icon() const;
    virtual QString overview() const = 0;
    virtual QString details() const = 0;
    virtual int errorCode() const = 0;
    virtual QString errorString() const = 0;
    virtual VisualCode code() const = 0;
    virtual AuditLog auditLog() const = 0;
    virtual QPointer<Task> parentTask() const {return QPointer<Task>();}

protected:
    static QString iconPath(VisualCode code);
    static QString makeOverview(const QString &msg);

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

}
}

#endif /* __KLEOPATRA_CRYPTO_TASK_H__ */

