/* -*- mode: c++; c-basic-offset:4 -*-
    commands/exportopenpgpcertstoservercommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "exportopenpgpcertstoservercommand.h"

#include "command_p.h"

#include <Libkleo/GnuPG>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include <KMessageBox>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

ExportOpenPGPCertsToServerCommand::ExportOpenPGPCertsToServerCommand(KeyListController *c)
    : GnuPGProcessCommand(c)
{

}

ExportOpenPGPCertsToServerCommand::ExportOpenPGPCertsToServerCommand(QAbstractItemView *v, KeyListController *c)
    : GnuPGProcessCommand(v, c)
{

}

ExportOpenPGPCertsToServerCommand::ExportOpenPGPCertsToServerCommand(const Key &key)
    : GnuPGProcessCommand(key)
{

}

ExportOpenPGPCertsToServerCommand::~ExportOpenPGPCertsToServerCommand() = default;

bool ExportOpenPGPCertsToServerCommand::preStartHook(QWidget *parent) const
{
    if (!haveKeyserverConfigured())
        if (KMessageBox::warningContinueCancel(parent,
                                               xi18nc("@info",
                                                       "<para>No OpenPGP directory services have been configured.</para>"
                                                       "<para>Since none is configured, <application>Kleopatra</application> will use "
                                                       "<resource>keys.gnupg.net</resource> as the server to export to.</para>"
                                                       "<para>You can configure OpenPGP directory servers in <application>Kleopatra</application>'s "
                                                       "configuration dialog.</para>"
                                                       "<para>Do you want to continue with <resource>keys.gnupg.net</resource> "
                                                       "as the server to export to?</para>"),
                                               i18nc("@title:window", "OpenPGP Certificate Export"),
                                               KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                                               QStringLiteral("warn-export-openpgp-missing-keyserver"))
                != KMessageBox::Continue) {
            return false;
        }
    return KMessageBox::warningContinueCancel(parent,
            xi18nc("@info",
                   "<para>When OpenPGP certificates have been exported to a public directory server, "
                   "it is nearly impossible to remove them again.</para>"
                   "<para>Before exporting your certificate to a public directory server, make sure that you "
                   "have created a revocation certificate so you can revoke the certificate if needed later.</para>"
                   "<para>Are you sure you want to continue?</para>"),
            i18nc("@title:window", "OpenPGP Certificate Export"),
            KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
            QStringLiteral("warn-export-openpgp-nonrevocable"))
           == KMessageBox::Continue;
}

QStringList ExportOpenPGPCertsToServerCommand::arguments() const
{
    QStringList result;
    result << gpgPath();
    if (!haveKeyserverConfigured()) {
        result << QStringLiteral("--keyserver") << QStringLiteral("keys.gnupg.net");
    }
    result << QStringLiteral("--send-keys");
    const auto keys = d->keys();
    for (const Key &key : keys) {
        result << QLatin1String(key.primaryFingerprint());
    }
    return result;
}

QString ExportOpenPGPCertsToServerCommand::errorCaption() const
{
    return i18nc("@title:window", "OpenPGP Certificate Export Error");
}

QString ExportOpenPGPCertsToServerCommand::successCaption() const
{
    return i18nc("@title:window", "OpenPGP Certificate Export Finished");
}

QString ExportOpenPGPCertsToServerCommand::crashExitMessage(const QStringList &args) const
{
    return xi18nc("@info",
                  "<para>The GPG process that tried to export OpenPGP certificates "
                  "ended prematurely because of an unexpected error.</para>"
                  "<para>Please check the output of <icode>%1</icode> for details.</para>", args.join(QLatin1Char(' ')));
}

QString ExportOpenPGPCertsToServerCommand::errorExitMessage(const QStringList &args) const
{
    return xi18nc("@info",
                  "<para>An error occurred while trying to export OpenPGP certificates.</para> "
                  "<para>The output from <command>%1</command> was: <message>%2</message></para>",
                  args[0], errorString());
}

QString ExportOpenPGPCertsToServerCommand::successMessage(const QStringList &) const
{
    return i18nc("@info", "OpenPGP certificates exported successfully.");
}

