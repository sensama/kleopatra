/* utils/windowsprocessdevice.h
    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2019 g10code GmbH

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

#if defined(WIN32) || defined(Q_MOC_RUN)
#ifndef WINDOWSPROCESSDEVICE_H
#define WINDOWSPROCESSDEVICE_H

#include <memory>

#include <QIODevice>

class QString;
class QStringList;


namespace Kleo
{
/* Simplistic anonymous pipe io device
 *
 * Create an IODevice using Windows Create Process and pipes
 * this class serves as an alternative to use QProcess on
 * Windows which event driven nature does not play well
 * in our threading and IPC model.
 *
 * This class was written with gpgtar in mind and mostly
 * a reaction to multiple issues we had with QProcess
 * and gpgtar in GPGME on Windows. It was so hard to debug them
 * that we decided for a simple approach that gives us
 * full control.
 *
 * As there are use cases streaming terrabytes through this
 * even the control of the buffer size is an advantage.
 *
 **/
class WindowsProcessDevice: public QIODevice
{
Q_OBJECT
public:
    WindowsProcessDevice(const QString &path, const QStringList &args, const QString &wd);

    /* Starts the process. Only supports
       QIODevice::ReadOnly
       QIODevice::WriteOnly
       QIODevice::ReadWrite */
    bool open(OpenMode mode) override;

    /* Terminates the process */
    void close() override;

    bool isSequential() const override;

    /* Closes the write channel */
    void closeWriteChannel();

    /* Get the an error string either stderr or a windows error */
    QString errorString();
protected:
    /* Blocking read */
    qint64 readData(char* data, qint64 maxSize) override;
    /* Blocking write */
    qint64 writeData(const char* data, qint64 size) override;
private:
    Q_DISABLE_COPY(WindowsProcessDevice)
    class Private;
    std::shared_ptr<Private> d;
};

} // namespace Kleo

#endif // WINDOWSPROCESSDEVICE_H
#endif // WIN32
