/*
  SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

  SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QIODevice>

#include <utility>

class KDPipeIODevice : public QIODevice
{
    Q_OBJECT
    //KDAB_MAKE_CHECKABLE( KDPipeIODevice )
public:
    enum DebugLevel {
        NoDebug,
        Debug
    };

    static DebugLevel debugLevel();
    static void setDebugLevel(DebugLevel level);

    explicit KDPipeIODevice(QObject *parent = nullptr);
    explicit KDPipeIODevice(int fd, OpenMode = ReadOnly, QObject *parent = nullptr);
    explicit KDPipeIODevice(Qt::HANDLE handle, OpenMode = ReadOnly, QObject *parent = nullptr);
    ~KDPipeIODevice() override;

    static std::pair<KDPipeIODevice *, KDPipeIODevice *> makePairOfConnectedPipes();

    bool open(int fd, OpenMode mode = ReadOnly);
    bool open(Qt::HANDLE handle, OpenMode mode = ReadOnly);

    Qt::HANDLE handle() const;
    int descriptor() const;

    bool readWouldBlock() const;
    bool writeWouldBlock() const;

    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;
    bool canReadLine() const override;
    void close() override;
    bool isSequential() const override;
    bool atEnd() const override;

    bool waitForBytesWritten(int msecs) override;
    bool waitForReadyRead(int msecs) override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    using QIODevice::open;

private:
    class Private;
    Private *d;
};


