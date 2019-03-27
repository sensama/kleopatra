/* utils/windowsprocessdevice.cpp
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

#ifdef WIN32
#include "windowsprocessdevice.h"

#include "kleopatra_debug.h"

#include "gnupg-helper.h"

#include <windows.h>

#include <QDir>

/* This is the amount of data GPGME reads at once */
#define PIPEBUF_SIZE 16384

using namespace Kleo;

static void CloseHandleX(HANDLE &h)
{
    if (h && h != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(h)) {
            qCWarning(KLEOPATRA_LOG) << "CloseHandle failed!";
        }
        h = nullptr;
    }
}

class WindowsProcessDevice::Private
{
public:
    ~Private();

    Private(const QString &path, const QStringList &args, const QString &wd):
        mPath(path),
        mArgs(args),
        mWorkingDirectory(wd),
        mStdInRd(nullptr),
        mStdInWr(nullptr),
        mStdOutRd(nullptr),
        mStdOutWr(nullptr),
        mStdErrRd(nullptr),
        mStdErrWr(nullptr),
        mProc(nullptr),
        mThread(nullptr),
        mEnded(false)
    {

    }

    bool start (QIODevice::OpenMode mode);

    qint64 write(const char *data, qint64 size)
    {
        if (size < 0 || (size >> 32)) {
            qCDebug (KLEOPATRA_LOG) << "Invalid write";
            return -1;
        }

        if (!mStdInWr) {
            qCDebug (KLEOPATRA_LOG) << "Write to closed or read only device";
            return -1;
        }

        DWORD dwWritten;
        if (!WriteFile(mStdInWr, data, (DWORD) size, &dwWritten, nullptr)) {
            qCDebug(KLEOPATRA_LOG) << "Failed to write";
            return -1;
        }
        if (dwWritten != size) {
            qCDebug(KLEOPATRA_LOG) << "Failed to write everything";
            return -1;
        }
        return size;
    }

    qint64 read(char *data, qint64 maxSize)
    {
        if (!mStdOutRd) {
            qCDebug (KLEOPATRA_LOG) << "Read of closed or write only device";
            return -1;
        }

        if (!maxSize) {
            return 0;
        }

        DWORD exitCode = 0;
        if (GetExitCodeProcess (mProc, &exitCode)) {
            if (exitCode != STILL_ACTIVE) {
                if (exitCode) {
                    qCDebug(KLEOPATRA_LOG) << "Non zero exit code";
                    mError = readAllStdErr();
                    return -1;
                }
                mEnded = true;
                qCDebug(KLEOPATRA_LOG) << "Process finished with code " << exitCode;
            }
        } else {
            qCDebug(KLEOPATRA_LOG) << "GetExitCodeProcess Failed";
        }

        if (mEnded) {
            DWORD avail = 0;
            if (!PeekNamedPipe(mStdOutRd,
                               nullptr,
                               0,
                               nullptr,
                               &avail,
                               nullptr)) {
                qCDebug(KLEOPATRA_LOG) << "Failed to peek pipe";
                return -1;
            }
            if (!avail) {
                qCDebug(KLEOPATRA_LOG) << "Process ended and nothing more in pipe";
                return 0;
            }
        }

        DWORD dwRead;
        if (!ReadFile(mStdOutRd, data, (DWORD) maxSize, &dwRead, nullptr)) {
            qCDebug(KLEOPATRA_LOG) << "Failed to read";
            return -1;
        }

        return dwRead;
    }

    QString readAllStdErr()
    {
        QString ret;
        if (!mStdErrRd) {
            qCDebug (KLEOPATRA_LOG) << "Read of closed stderr";
        }
        DWORD dwRead = 0;
        do {
            char buf[4096];
            DWORD avail;
            if (!PeekNamedPipe(mStdErrRd,
                               nullptr,
                               0,
                               nullptr,
                               &avail,
                               nullptr)) {
                qCDebug(KLEOPATRA_LOG) << "Failed to peek pipe";
                return ret;
            }
            if (!avail) {
                return ret;
            }
            ReadFile(mStdErrRd, buf, 4096, &dwRead, nullptr);
            if (dwRead) {
                QByteArray ba (buf, dwRead);
                ret += QString::fromLocal8Bit (ba);
            }
        } while (dwRead);
        return ret;
    }

    void close()
    {
        if (mProc && mProc != INVALID_HANDLE_VALUE) {
            TerminateProcess(mProc, 0xf291);
            CloseHandleX(mProc);
        }
    }

    QString errorString()
    {
        return mError;
    }

    void closeWriteChannel()
    {
        CloseHandleX(mStdInWr);
    }

private:
    QString mPath;
    QStringList mArgs;
    QString mWorkingDirectory;
    QString mError;
    HANDLE mStdInRd;
    HANDLE mStdInWr;
    HANDLE mStdOutRd;
    HANDLE mStdOutWr;
    HANDLE mStdErrRd;
    HANDLE mStdErrWr;
    HANDLE mProc;
    HANDLE mThread;
    bool mEnded;
};

WindowsProcessDevice::WindowsProcessDevice(const QString &path,
        const QStringList &args,
        const QString &wd):
    d(new Private(path, args, wd))
{

}

bool WindowsProcessDevice::open(QIODevice::OpenMode mode)
{
    bool ret = d->start(mode);
    if (ret) {
        setOpenMode(mode);
    }
    return ret;
}

qint64 WindowsProcessDevice::readData(char *data, qint64 maxSize)
{
    return d->read(data, maxSize);
}

qint64 WindowsProcessDevice::writeData(const char *data, qint64 maxSize)
{
    return d->write(data, maxSize);
}

bool WindowsProcessDevice::isSequential() const
{
    return true;
}

void WindowsProcessDevice::closeWriteChannel()
{
    d->closeWriteChannel();
}

void WindowsProcessDevice::close()
{
    d->close();
    QIODevice::close();
}

QString getLastErrorString()
{
    wchar_t *lpMsgBuf = nullptr;
    DWORD dw = GetLastError();

    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (wchar_t *) &lpMsgBuf,
        0, NULL);

    QString ret = QString::fromWCharArray(lpMsgBuf);

    LocalFree(lpMsgBuf);
    return ret;
}

WindowsProcessDevice::Private::~Private()
{
    if (mProc && mProc != INVALID_HANDLE_VALUE) {
        close();
    }
    CloseHandleX (mThread);
    CloseHandleX (mStdInRd);
    CloseHandleX (mStdInWr);
    CloseHandleX (mStdOutRd);
    CloseHandleX (mStdOutWr);
    CloseHandleX (mStdErrRd);
    CloseHandleX (mStdErrWr);
}

static QString qt_create_commandline(const QString &program, const QStringList &arguments,
                                     const QString &nativeArguments)
{
    QString args;
    if (!program.isEmpty()) {
        QString programName = program;
        if (!programName.startsWith(QLatin1Char('\"')) && !programName.endsWith(QLatin1Char('\"')) && programName.contains(QLatin1Char(' ')))
            programName = QLatin1Char('\"') + programName + QLatin1Char('\"');
        programName.replace(QLatin1Char('/'), QLatin1Char('\\'));

        // add the prgram as the first arg ... it works better
        args = programName + QLatin1Char(' ');
    }

    for (int i=0; i<arguments.size(); ++i) {
        QString tmp = arguments.at(i);
        // Quotes are escaped and their preceding backslashes are doubled.
        tmp.replace(QRegExp(QLatin1String("(\\\\*)\"")), QLatin1String("\\1\\1\\\""));
        if (tmp.isEmpty() || tmp.contains(QLatin1Char(' ')) || tmp.contains(QLatin1Char('\t'))) {
            // The argument must not end with a \ since this would be interpreted
            // as escaping the quote -- rather put the \ behind the quote: e.g.
            // rather use "foo"\ than "foo\"
            int i = tmp.length();
            while (i > 0 && tmp.at(i - 1) == QLatin1Char('\\'))
                --i;
            tmp.insert(i, QLatin1Char('"'));
            tmp.prepend(QLatin1Char('"'));
        }
        args += QLatin1Char(' ') + tmp;
    }

    if (!nativeArguments.isEmpty()) {
        if (!args.isEmpty())
             args += QLatin1Char(' ');
        args += nativeArguments;
    }

    return args;
}

bool WindowsProcessDevice::Private::start(QIODevice::OpenMode mode)
{
    if (mode != QIODevice::ReadOnly &&
        mode != QIODevice::WriteOnly &&
        mode != QIODevice::ReadWrite) {
        qCDebug(KLEOPATRA_LOG) << "Unsupported open mode " << mode;
        return false;
    }

    SECURITY_ATTRIBUTES saAttr;
    ZeroMemory (&saAttr, sizeof (SECURITY_ATTRIBUTES));
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create the pipes
    if (!CreatePipe(&mStdOutRd, &mStdOutWr, &saAttr, PIPEBUF_SIZE) ||
        !CreatePipe(&mStdErrRd, &mStdErrWr, &saAttr, 0) ||
        !CreatePipe(&mStdInRd, &mStdInWr, &saAttr, PIPEBUF_SIZE)) {
        qCDebug(KLEOPATRA_LOG) << "Failed to create pipes";
        mError = getLastErrorString();
        return false;
    }

    // Ensure only the proper handles are inherited
    if (!SetHandleInformation(mStdOutRd, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(mStdErrRd, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(mStdInWr,  HANDLE_FLAG_INHERIT, 0)) {

        qCDebug(KLEOPATRA_LOG) << "Failed to set inherit flag";
        mError = getLastErrorString();
        return false;
    }

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = mStdErrWr;
    siStartInfo.hStdOutput = mStdOutWr;
    siStartInfo.hStdInput = mStdInRd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    const auto args = qt_create_commandline(mPath, mArgs, QString());
    wchar_t *cmdLine = wcsdup (reinterpret_cast<const wchar_t *>(args.utf16()));
    const wchar_t *proc = reinterpret_cast<const wchar_t *>(mPath.utf16());
    const QString nativeWorkingDirectory = QDir::toNativeSeparators(mWorkingDirectory);
    const wchar_t *wd = reinterpret_cast<const wchar_t *>(nativeWorkingDirectory.utf16());

    qCDebug(KLEOPATRA_LOG) << "Spawning:" << args;
    // Now lets start
    bool suc = CreateProcessW(proc,
                              cmdLine,       // command line
                              NULL,          // process security attributes
                              NULL,          // primary thread security attributes
                              TRUE,          // handles are inherited
                              CREATE_NO_WINDOW,// creation flags
                              NULL,          // use parent's environment
                              wd,          // use parent's current directory
                              &siStartInfo,  // STARTUPINFO pointer
                              &piProcInfo);  // receives PROCESS_INFORMATION

    free(cmdLine);
    if (!suc) {
        qCDebug(KLEOPATRA_LOG) << "Failed to create process";
        mError = getLastErrorString();
        return false;
    }

    mProc = piProcInfo.hProcess;
    mThread = piProcInfo.hThread;

    if (mode == QIODevice::WriteOnly) {
        CloseHandleX (mStdInRd);
    }

    if (mode == QIODevice::ReadOnly) {
        CloseHandleX (mStdInWr);
    }

    return true;
}

QString WindowsProcessDevice::errorString()
{
    return d->errorString();
}

#include "windowsprocessdevice.moc"
#endif
