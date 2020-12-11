/* -*- mode: c++; c-basic-offset:4 -*-
    utils/output.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UTILS_OUTPUT_H__
#define __KLEOPATRA_UTILS_OUTPUT_H__

#include <kleo-assuan.h> // for assuan_fd_t

#include <utils/pimpl_ptr.h>

#include <memory>

class QIODevice;
class QString;
#include <QStringList>
class QDir;
class QWidget;

namespace Kleo
{

class OverwritePolicy
{
public:
    enum Policy {
        Allow,
        Deny,
        Ask
    };

    explicit OverwritePolicy(QWidget *parent, Policy initialPolicy = Ask);
    ~OverwritePolicy();

    Policy policy() const;
    void setPolicy(Policy);

    QWidget *parentWidget() const;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

class Output
{
public:
    virtual ~Output();

    virtual void setLabel(const QString &label) = 0;
    virtual QString label() const = 0;
    virtual std::shared_ptr<QIODevice> ioDevice() const = 0;
    virtual QString errorString() const = 0;
    virtual bool isFinalized() const = 0;
    virtual void finalize() = 0;
    virtual void cancel() = 0;
    virtual bool binaryOpt() const = 0;
    virtual void setBinaryOpt(bool value) = 0;
    /** Whether or not the output failed. */
    virtual bool failed() const { return false; }

    static std::shared_ptr<Output> createFromFile(const QString &fileName, const std::shared_ptr<OverwritePolicy> &);
    static std::shared_ptr<Output> createFromFile(const QString &fileName, bool forceOverwrite);
    static std::shared_ptr<Output> createFromPipeDevice(assuan_fd_t fd, const QString &label);
    static std::shared_ptr<Output> createFromProcessStdIn(const QString &command);
    static std::shared_ptr<Output> createFromProcessStdIn(const QString &command, const QStringList &args);
    static std::shared_ptr<Output> createFromProcessStdIn(const QString &command, const QStringList &args, const QDir &workingDirectory);
#ifndef QT_NO_CLIPBOARD
    static std::shared_ptr<Output> createFromClipboard();
#endif
    static std::shared_ptr<Output> createFromByteArray(QByteArray *data, const QString &label);
};
}

#endif /* __KLEOPATRA_UTILS_OUTPUT_H__ */

