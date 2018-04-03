/* -*- mode: c++; c-basic-offset:4 -*-
    iodevicelogger.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#ifndef __KLEOPATRA_IODEVICELOGGER_H__
#define __KLEOPATRA_IODEVICELOGGER_H__

#include <QIODevice>

#include <utils/pimpl_ptr.h>

#include <memory>

namespace Kleo
{

class IODeviceLogger : public QIODevice
{
    Q_OBJECT
public:
    explicit IODeviceLogger(const std::shared_ptr<QIODevice> &iod, QObject *parent = nullptr);
    ~IODeviceLogger() override;

    void setWriteLogDevice(const std::shared_ptr<QIODevice> &dev);
    void setReadLogDevice(const std::shared_ptr<QIODevice> &dev);

    bool atEnd() const override;
    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;
    bool canReadLine() const override;
    void close() override;
    bool isSequential() const override;
    bool open(OpenMode mode) override;
    qint64 pos() const override;
    bool reset() override;
    bool seek(qint64 pos) override;
    qint64 size() const override;
    bool waitForBytesWritten(int msecs) override;
    bool waitForReadyRead(int msecs) override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
    qint64 readLineData(char *data, qint64 maxSize) override;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};
}

#endif // __KLEOPATRA_IODEVICELOGGER_H__
