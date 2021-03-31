/* -*- mode: c++; c-basic-offset:4 -*-
    core/command.h

    This file is part of KleopatraClient, the Kleopatra interface library
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "kleopatraclientcore_export.h"

#include <QObject>
#include <QWidget> // only for WId, doesn't prevent linking against QtCore-only

class QString;
class QByteArray;
class QVariant;

namespace KleopatraClientCopy
{

class KLEOPATRACLIENTCORE_EXPORT Command : public QObject
{
    Q_OBJECT
public:
    explicit Command(QObject *parent = nullptr);
    ~Command();

    void setParentWId(WId wid);
    WId parentWId() const;

    void setServerLocation(const QString &location);
    QString serverLocation() const;

    bool waitForFinished();
    bool waitForFinished(unsigned long ms);

    bool error() const;
    bool wasCanceled() const;
    QString errorString() const;

    qint64 serverPid() const;

public Q_SLOTS:
    void start();
    void cancel();

Q_SIGNALS:
    void started();
    void finished();

protected:
    void setOptionValue(const char *name, const QVariant &value, bool critical = true);
    void setOption(const char *name, bool critical = true);
    void unsetOption(const char *name);

    QVariant optionValue(const char *name) const;
    bool isOptionSet(const char *name) const;
    bool isOptionCritical(const char *name) const;

    void setFilePaths(const QStringList &filePaths);
    QStringList filePaths() const;

    void setRecipients(const QStringList &recipients, bool informative);
    QStringList recipients() const;
    bool areRecipientsInformative() const;

    void setSenders(const QStringList &senders, bool informative);
    QStringList senders() const;
    bool areSendersInformative() const;

    void setInquireData(const char *what, const QByteArray &data);
    void unsetInquireData(const char *what);
    QByteArray inquireData(const char *what) const;
    bool isInquireDataSet(const char *what) const;

    QByteArray receivedData() const;

    void setCommand(const char *command);
    QByteArray command() const;

protected:
    class Private;
    Private *d;
    Command(Private *p, QObject *parent);
};

}

