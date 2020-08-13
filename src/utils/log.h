/* -*- mode: c++; c-basic-offset:4 -*-
    utils/log.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_LOG_H__
#define __KLEOPATRA_LOG_H__

#include <utils/pimpl_ptr.h>

#include  <memory>

#include <cstdio>

class QIODevice;
class QString;

namespace Kleo
{

class Log
{
public:

    enum OpenMode {
        Read = 0x1,
        Write = 0x2
    };

    static void messageHandler(QtMsgType type, const QMessageLogContext &ctx,
                               const QString &msg);

    static std::shared_ptr<const Log> instance();
    static std::shared_ptr<Log> mutableInstance();

    ~Log();

    bool ioLoggingEnabled() const;
    void setIOLoggingEnabled(bool enabled);

    QString outputDirectory() const;
    void setOutputDirectory(const QString &path);

    std::shared_ptr<QIODevice> createIOLogger(const std::shared_ptr<QIODevice> &wrapped, const QString &prefix, OpenMode mode) const;

    FILE *logFile() const;

private:
    Log();

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;

    Q_DISABLE_COPY(Log)
};

}

#endif // __KLEOPATRA_LOG_H__
