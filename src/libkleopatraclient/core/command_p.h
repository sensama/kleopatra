/* -*- mode: c++; c-basic-offset:4 -*-
    command_p.h

    This file is part of KleopatraClient, the Kleopatra interface library
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef __LIBKLEOPATRACLIENT_CORE_COMMAND_P_H__
#define __LIBKLEOPATRACLIENT_CORE_COMMAND_P_H__

#include "command.h"

#include <QThread>
#include <QMutex>

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QVariant>

#include <map>
#include <string>

class KleopatraClientCopy::Command::Private : public QThread
{
    Q_OBJECT
private:
    friend class ::KleopatraClientCopy::Command;
    Command *const q;
public:
    explicit Private(Command *qq)
        : QThread(),
          q(qq),
          mutex(QMutex::Recursive),
          inputs(),
          outputs()
    {

    }
    ~Private() override {}

private:
    void init();

private:
    void run() override;

private:
    QMutex mutex;
    struct Option {
        QVariant value;
        bool hasValue : 1;
        bool isCritical : 1;
    };
    struct Inputs {
        Inputs() : parentWId(0), areRecipientsInformative(false), areSendersInformative(false) {}
        std::map<std::string, Option> options;
        QStringList filePaths;
        QStringList recipients, senders;
        std::map<std::string, QByteArray> inquireData;
        WId parentWId;
        QByteArray command;
        bool areRecipientsInformative : 1;
        bool areSendersInformative    : 1;
    } inputs;
    struct Outputs {
        Outputs() : canceled(false), serverPid(0) {}
        QString errorString;
        bool canceled : 1;
        QByteArray data;
        qint64 serverPid;
        QString serverLocation;
    } outputs;
};

#endif /* __LIBKLEOPATRACLIENT_CORE_COMMAND_P_H__ */
