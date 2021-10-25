/*  smartcard/deviceinfowatcher_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <gpgme++/interfaces/statusconsumer.h>

#include <gpgme++/context.h>

#include <gpgme.h>

#include <QThread>

#include <memory>

namespace Kleo
{

class DeviceInfoWatcher::Worker : public QObject, public GpgME::StatusConsumer
{
    Q_OBJECT

public:
    ~Worker() override;

public Q_SLOTS:
    void start();

Q_SIGNALS:
    void statusChanged(const QByteArray &details);

private:
    Q_INVOKABLE void poll();

    void status(const char *status, const char *details) override;

private:
    int mRetryDelay = 100;
    std::unique_ptr<GpgME::Context> mContext;
};

class DeviceInfoWatcher::Private
{
    friend class ::Kleo::DeviceInfoWatcher;
    DeviceInfoWatcher *const q;
public:
    explicit Private(DeviceInfoWatcher *qq);
    ~Private();

private:
    void start();

private:
    QThread workerThread;
};

}

