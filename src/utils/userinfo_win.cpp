/*
    utils/userinfo_win.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "userinfo_win_p.h"
#include "kleopatra_debug.h"

/* Use Windows API to query the user name and email.
   EXTENDED_NAME_FORMAT is documented in MSDN */
QString win_get_user_name(EXTENDED_NAME_FORMAT what)
{
  QString ret;
  wchar_t tmp[1];
  ULONG nSize = 1;
  if (what == NameUnknown) {
      if (GetUserNameW (tmp, &nSize)) {
          qCWarning (KLEOPATRA_LOG) << "Got empty username";
          return ret;
      }
  } else if (GetUserNameExW (what, tmp, &nSize)) {
      return ret;
  }

  /* nSize now contains the required size of the buffer */
  wchar_t *buf = new wchar_t[nSize];

  if (what == NameUnknown) {
      if (!GetUserNameW (buf, &nSize)) {
          qCWarning (KLEOPATRA_LOG) << "Failed to get username";
          delete[] buf;
          return ret;
      }
  } else if (!GetUserNameExW (what, buf, &nSize)) {
      delete[] buf;
      return ret;
  }
  ret = QString::fromWCharArray (buf, nSize);
  delete[] buf;
  return ret.trimmed();
}
