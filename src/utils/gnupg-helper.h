/* -*- mode: c++; c-basic-offset:4 -*-
    utils/gnupg-helper.h

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

#ifndef __KLEOPATRA_GNUPGHELPER_H__
#define __KLEOPATRA_GNUPGHELPER_H__

#include <gpgme++/engineinfo.h>
#include <gpgme++/key.h>

/* Support compilation with GPGME older than 1.9.  */
# define GPGME_HAS_KEY_IS_DEVS

/* Does the given object comply with DE_VS?  This macro can be used to
   ensure that we can still build against older versions of GPGME
   without cluttering the code with preprocessor conditionals.  */
# define IS_DE_VS(x)	(x).isDeVs()

class QString;
class QStringList;
class QByteArray;

namespace Kleo
{

QString gnupgHomeDirectory();

QString gpgConfPath();
QString gpgSmPath();
QString gpgPath();

QString gpgConfListDir(const char *which);
QString gpg4winInstallPath();
QString gpg4winVersion();
QString gnupgInstallPath();
const QString& paperKeyInstallPath();

QStringList gnupgFileWhitelist();
int makeGnuPGError(int code);

bool engineIsVersion(int major, int minor, int patch, GpgME::Engine = GpgME::GpgConfEngine);
bool haveKeyserverConfigured();
bool gpgComplianceP(const char *mode);
enum GpgME::UserID::Validity keyValidity(const GpgME::Key &key);

/* Convert GnuPG output to a QString with proper encoding.
 * Takes Gpg Quirks into account and might handle future
 * changes in GnuPG Output. */
QString stringFromGpgOutput(const QByteArray &ba);

/* Check if a minimum version is there. Strings should be in the format:
 * 1.2.3 */

bool versionIsAtLeast(const char *minimum, const char *actual);
}

#endif // __KLEOPATRA_GNUPGHELPER_H__
