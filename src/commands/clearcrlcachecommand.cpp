/* -*- mode: c++; c-basic-offset:4 -*-
    commands/clearcrlcachecommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "clearcrlcachecommand.h"
#include <Libkleo/GnuPG>

#include <KLocalizedString>

using namespace Kleo;
using namespace Kleo::Commands;

ClearCrlCacheCommand::ClearCrlCacheCommand(KeyListController *c)
    : GnuPGProcessCommand(c)
{

}

ClearCrlCacheCommand::ClearCrlCacheCommand(QAbstractItemView *v, KeyListController *c)
    : GnuPGProcessCommand(v, c)
{

}

ClearCrlCacheCommand::~ClearCrlCacheCommand() {}

QStringList ClearCrlCacheCommand::arguments() const
{
#ifdef Q_OS_WIN
    return QStringList() << gpgSmPath()
                         << QStringLiteral("--call-dirmngr")
                         << QStringLiteral("flushcrls");
#else
    // TODO: Replace with a version check if an unpatched
    // gnupg supports it otherwise this mostly works on
    // GNU/Linux but on Windows this did not work
    return QStringList() << QStringLiteral("dirmngr") << QStringLiteral("--flush");
#endif
}

QString ClearCrlCacheCommand::errorCaption() const
{
    return i18nc("@title:window", "Clear CRL Cache Error");
}

QString ClearCrlCacheCommand::successCaption() const
{
    return i18nc("@title:window", "Clear CRL Cache Finished");
}

QString ClearCrlCacheCommand::crashExitMessage(const QStringList &args) const
{
    return i18n("The DirMngr process that tried to clear the CRL cache "
                "ended prematurely because of an unexpected error. "
                "Please check the output of %1 for details.", args.join(QLatin1Char(' ')));
}

QString ClearCrlCacheCommand::errorExitMessage(const QStringList &args) const
{
    return i18n("An error occurred while trying to clear the CRL cache. "
                "The output from %1 was:\n%2", args[0], errorString());
}

QString ClearCrlCacheCommand::successMessage(const QStringList &) const
{
    return i18n("CRL cache cleared successfully.");
}

