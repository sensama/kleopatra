/* -*- mode: c++; c-basic-offset:4 -*-
    utils/gnupg-helper.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
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
