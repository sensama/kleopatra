/*
    kuniqueservice_win.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2016 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

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

#include "kuniqueservice.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>

#include "kleopatra_debug.h"
#include <windows.h>

#define MY_DATA_TYPE 12

class KUniqueService::KUniqueServicePrivate
{
    Q_DECLARE_PUBLIC(KUniqueService)
    Q_DISABLE_COPY(KUniqueServicePrivate)

private:
    KUniqueService *q_ptr;
    HWND mResponder;
    HANDLE mCurrentProcess;

    const QString getWindowName() const
    {
        return QCoreApplication::applicationName() + QStringLiteral("Responder");
    }

    HWND getForeignResponder() const
    {
        const QString qWndName = getWindowName();
        wchar_t *wndName = (wchar_t *)qWndName.utf16();
        HWND ret = FindWindow(wndName, wndName);
        qCDebug(KLEOPATRA_LOG) << "Responder handle:" << ret;
        return ret;
    }

    void createResponder()
    {
        WNDCLASS windowClass;
        const QString qWndName = getWindowName();
        wchar_t *wndName = (wchar_t*)qWndName.utf16();
        windowClass.style = CS_GLOBALCLASS | CS_DBLCLKS;
        windowClass.lpfnWndProc = windowProc;
        windowClass.hInstance   = (HINSTANCE) GetModuleHandle(NULL);
        windowClass.lpszClassName = wndName;
        windowClass.hIcon = nullptr;
        windowClass.hCursor = nullptr;
        windowClass.hbrBackground = nullptr;
        windowClass.lpszMenuName = nullptr;
        windowClass.cbClsExtra = 0;
        windowClass.cbWndExtra = 0;
        RegisterClass(&windowClass);
        mResponder = CreateWindow(wndName, wndName,
                                  0, 0, 0, 10, 10, nullptr, nullptr,
                                  (HINSTANCE)GetModuleHandle(NULL), nullptr);
        qCDebug(KLEOPATRA_LOG) << "Created responder: " << qWndName
                               << " with handle: " << mResponder;
    }

    void handleRequest(const COPYDATASTRUCT *cds)
    {
        Q_Q(KUniqueService);
        if (cds->dwData != MY_DATA_TYPE) {
            qCDebug(KLEOPATRA_LOG) << "Responder called with invalid data type:"
                                   << cds->dwData;
            return;
        }
        if (mCurrentProcess) {
            qCDebug(KLEOPATRA_LOG) << "Already serving. Terminating process: "
                                   << mCurrentProcess;
            setExitValue(42);
        }
        const QByteArray serialized(static_cast<const char*>(cds->lpData),
                                    cds->cbData);
        QDataStream ds(serialized);
        quint32 curProc;
        ds >> curProc;
        mCurrentProcess = (HANDLE) curProc;
        QString workDir;
        ds >> workDir;
        QStringList args;
        ds >> args;
        qCDebug(KLEOPATRA_LOG) << "Process handle: " << mCurrentProcess
                               << " requests activate with args "
                               << args;
        q->emitActivateRequested(args, workDir);
        return;
    }

    KUniqueServicePrivate(KUniqueService *q) : q_ptr(q), mResponder(nullptr),
                                               mCurrentProcess(nullptr)
    {
        HWND responder = getForeignResponder();
        if (!responder) {
            // We are the responder
            createResponder();
            return;
        }
        // We are the client

        QByteArray serialized;
        QDataStream ds(&serialized, QIODevice::WriteOnly);
        DWORD targetId = 0;
        GetWindowThreadProcessId(responder, &targetId);
        if (!targetId) {
            qCDebug(KLEOPATRA_LOG) << "No process id of responder window";
            return;
        }
        HANDLE targetHandle = OpenProcess(PROCESS_DUP_HANDLE, FALSE, targetId);
        if (!targetHandle) {
            qCDebug(KLEOPATRA_LOG) << "No target handle. Err: " << GetLastError();
        }

        // To allow the other process to terminate the process
        // needs a handle to us with the required access.
        if (!DuplicateHandle(GetCurrentProcess(),
                             GetCurrentProcess(),
                             targetHandle,
                             &mCurrentProcess,
                             0,
                             FALSE,
                             DUPLICATE_SAME_ACCESS)) {
            qCDebug(KLEOPATRA_LOG) << "Failed to duplicate handle";
        }
        CloseHandle(targetHandle);

        ds << (qint32) mCurrentProcess
           << QDir::currentPath()
           << QCoreApplication::arguments();
        COPYDATASTRUCT cds;
        cds.dwData = MY_DATA_TYPE;
        cds.cbData = serialized.size();
        cds.lpData = serialized.data();

        qCDebug(KLEOPATRA_LOG) << "Sending message to existing Window.";
        SendMessage(responder, WM_COPYDATA, 0, (LPARAM) &cds);
        // Usually we should be terminated while sending the message.
        qCDebug(KLEOPATRA_LOG) << "Send message returned.";
    }

    static KUniqueServicePrivate *instance(KUniqueService *q) {
        static KUniqueServicePrivate *self;
        if (self) {
            return self;
        }

        self = new KUniqueServicePrivate(q);
        return self;
    }

    static LRESULT CALLBACK windowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_COPYDATA)
        {
            const COPYDATASTRUCT *cds = (COPYDATASTRUCT*)lParam;
            // windowProc must be static so the singleton pattern although
            // it doesn't make much sense in here.
            instance(nullptr)->handleRequest(cds);
            return 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    ~KUniqueServicePrivate()
    {
        if (mResponder) {
            DestroyWindow(mResponder);
        }
    }

    void setExitValue(int code)
    {
        TerminateProcess(mCurrentProcess, (unsigned int) code);
        mCurrentProcess = nullptr;
    }
};


KUniqueService::KUniqueService() : d_ptr(KUniqueServicePrivate::instance(this))
{
}

KUniqueService::~KUniqueService()
{
    delete d_ptr;
}

void KUniqueService::setExitValue(int code)
{
    Q_D(KUniqueService);
    d->setExitValue(code);
}
