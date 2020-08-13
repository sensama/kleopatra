/*  crypto/gui/signencryptemailconflictdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "gui-helper.h"

#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

/* This is a Hack to workaround the fact that Foregrounding
   a Window is so restricted that it AllowSetForegroundWindow
   does not always work e.g. when the ForegroundWindow timeout
   has not expired. This Hack is semi official but may stop
   working in future windows versions.

   This is similar to what pinentry-qt does on Windows.
   */
#ifdef Q_OS_WIN
WINBOOL SetForegroundWindowEx(HWND hWnd)
{
    //Attach foreground window thread to our thread
    const DWORD ForeGroundID = GetWindowThreadProcessId(::GetForegroundWindow(), NULL);
    const DWORD CurrentID   = GetCurrentThreadId();
    WINBOOL retval;

    AttachThreadInput(ForeGroundID, CurrentID, TRUE);
    //Do our stuff here
    HWND hLastActivePopupWnd = GetLastActivePopup(hWnd);
    retval = SetForegroundWindow(hLastActivePopupWnd);

    //Detach the attached thread
    AttachThreadInput(ForeGroundID, CurrentID, FALSE);
    return retval;
}// End SetForegroundWindowEx
#endif

void Kleo::aggressive_raise(QWidget *w, bool stayOnTop)
{
    /* Maybe Qt will become aggressive enough one day that
     * this is enough on windows too*/
    w->raise();
    w->setWindowState(Qt::WindowActive);
    w->activateWindow();
#ifdef Q_OS_WIN
    HWND wid = (HWND)w->effectiveWinId();
    /* In the meantime we do our own attention grabbing */
    if (!SetForegroundWindow(wid) && !SetForegroundWindowEx(wid)) {
        OutputDebugStringA("SetForegroundWindow (ex) failed");
        /* Yet another fallback which will not work on some
         * versions and is not recommended by msdn */
        if (!ShowWindow(wid, SW_SHOWNORMAL)) {
            OutputDebugStringA("ShowWindow failed.");
        }
    }
    /* Even if SetForgeoundWindow / SetForegroundWinowEx don't fail
     * we sometimes are still not in the foreground. So we try yet
     * another hack by using SetWindowPos */
    if (!SetWindowPos(wid, HWND_TOPMOST, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW)) {
        OutputDebugStringA("SetWindowPos failed.");
    }
    /* sometimes we want to stay on top even if the user
     * changes focus because we are _aggressive_ and otherwise
     * Outlook might show the "Help I'm unresponsive so I must have
     * crashed" Popup if the user clicks into Outlook while a dialog
     * from us is active. */
     else if (!stayOnTop) {
        // Without moving back to NOTOPMOST we just stay on top.
        // Even if the user changes focus.
        SetWindowPos(wid, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
#else
    Q_UNUSED(stayOnTop);
#endif
}

