/*  smartcard/deviceinfowatcher.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "deviceinfowatcher.h"
#include "deviceinfowatcher_p.h"

#include <QGpgME/Debug>

#include <gpgme++/context.h>
#include <gpgme++/engineinfo.h>
#include <gpgme++/statusconsumerassuantransaction.h>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace GpgME;

DeviceInfoWatcher::Worker::~Worker()
{
    if (mContext) {
        mContext->cancelPendingOperationImmediately();
    }
}

void DeviceInfoWatcher::Worker::start()
{
    if (!mContext) {
        Error err;
        mContext = Context::createForEngine(AssuanEngine, &err);
        if (err) {
            qCWarning(KLEOPATRA_LOG) << "DeviceInfoWatcher::Worker::start: Creating context failed:" << err;
            return;
        }
    }

    // try to connect to the agent for at most ~12.8 seconds with increasing delay between retries
    static const int MaxRetryDelay = 100 * 64;
    static const char *command = "SCD DEVINFO --watch";
    std::unique_ptr<AssuanTransaction> t(new StatusConsumerAssuanTransaction(this));
    const Error err = mContext->startAssuanTransaction(command, std::move(t));
    if (!err) {
        qCDebug(KLEOPATRA_LOG) << "DeviceInfoWatcher::Worker::start: Assuan transaction for" << command << "started";
        QMetaObject::invokeMethod(this, "poll", Qt::QueuedConnection);
        return;
    } else if (err.code() == GPG_ERR_ASS_CONNECT_FAILED) {
        if (mRetryDelay <= MaxRetryDelay) {
            qCInfo(KLEOPATRA_LOG) << "DeviceInfoWatcher::Worker::start: Connecting to the agent failed. Retrying in" << mRetryDelay << "ms";
            QThread::msleep(mRetryDelay);
            mRetryDelay *= 2;
            QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
            return;
        }
        qCWarning(KLEOPATRA_LOG) << "DeviceInfoWatcher::Worker::start: Connecting to the agent failed too often. Giving up.";
    } else {
        qCWarning(KLEOPATRA_LOG) << "DeviceInfoWatcher::Worker::start: Starting Assuan transaction for" << command << "failed:" << err;
    }
}

void DeviceInfoWatcher::Worker::poll()
{
    const bool finished = mContext->poll();
    if (finished) {
        qCDebug(KLEOPATRA_LOG) << "DeviceInfoWatcher::Worker::poll: context finished with" << mContext->lastError();
        QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(this, "poll", Qt::QueuedConnection);
    }
}

void DeviceInfoWatcher::Worker::status(const char* status, const char* details)
{
    qCDebug(KLEOPATRA_LOG) << "DeviceInfoWatcher::Worker::status:" << status << details;
    if (status && std::strcmp(status, "DEVINFO_STATUS") == 0) {
        Q_EMIT statusChanged(QByteArray(details));
    }
}

DeviceInfoWatcher::Private::Private(DeviceInfoWatcher *qq)
    : q(qq)
{
}

DeviceInfoWatcher::Private::~Private()
{
    workerThread.quit();
    workerThread.wait();
}

void DeviceInfoWatcher::Private::start()
{
    DeviceInfoWatcher::Worker *worker = new DeviceInfoWatcher::Worker;
    worker->moveToThread(&workerThread);
    connect(&workerThread, &QThread::started, worker, &DeviceInfoWatcher::Worker::start);
    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &DeviceInfoWatcher::Worker::statusChanged,
            q, &DeviceInfoWatcher::statusChanged);
    workerThread.start();
}

DeviceInfoWatcher::DeviceInfoWatcher(QObject *parent)
    : QObject(parent)
    , d(new Private(this))
{
}

DeviceInfoWatcher::~DeviceInfoWatcher()
{
    delete d;
}

// static
bool DeviceInfoWatcher::isSupported()
{
    return engineInfo(GpgEngine).engineVersion() >= "2.3.0";
}

void DeviceInfoWatcher::start()
{
    d->start();
}
