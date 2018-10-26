/*  crypto/gui/signencryptemailconflictdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2017 Intevation GmbH

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

