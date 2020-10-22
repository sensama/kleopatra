/*  smartcard/deviceinfowatcher.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef __KLEOPATRA_SMARTCARD_DEVICEINFOWATCHER_H__
#define __KLEOPATRA_SMARTCARD_DEVICEINFOWATCHER_H__

#include <QObject>

namespace Kleo
{

class DeviceInfoWatcher : public QObject
{
    Q_OBJECT
public:
    explicit DeviceInfoWatcher(QObject *parent = nullptr);
    ~DeviceInfoWatcher();

    static bool isSupported();

    void start();

Q_SIGNALS:
    void statusChanged(const QByteArray &details);

private:
    class Worker;

private:
    class Private;
    Private * const d;
};

}

#endif // __KLEOPATRA_SMARTCARD_DEVICEINFOWATCHER_H__
