/* -*- mode: c++; c-basic-offset:4 -*-
    commands/exportsecretkeycommand_old.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "exportsecretkeycommand_old.h"

#include "fileoperationspreferences.h"

#include "command_p.h"

#include <Libkleo/GnuPG>
#include <utils/filedialog.h>

#include <Libkleo/Classify>
#include <Libkleo/Formatting>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include <QFile>
#include <QProcess>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

namespace Kleo::Commands::Compat
{

ExportSecretKeyCommand::ExportSecretKeyCommand(QAbstractItemView *v, KeyListController *c)
    : GnuPGProcessCommand(v, c)
{
}

ExportSecretKeyCommand::ExportSecretKeyCommand(const Key &key)
    : GnuPGProcessCommand(key)
{
}

ExportSecretKeyCommand::~ExportSecretKeyCommand()
{
}

bool ExportSecretKeyCommand::preStartHook(QWidget *parent) const
{
    if (!m_filename.isEmpty()) {
        return true;
    }

    const auto key = d->key();

    const auto protocol = key.protocol();

    QString proposedFileName;
    const bool usePGPFileExt = FileOperationsPreferences().usePGPFileExt();
    auto name = Formatting::prettyName(key);
    if (name.isEmpty()) {
        name = Formatting::prettyEMail(key);
    }
    /* Not translated so it's better to use in tutorials etc. */
    proposedFileName = QStringLiteral("%1_%2_SECRET.%3")
                           .arg(name)
                           .arg(Formatting::prettyKeyID(key.shortKeyID()))
                           .arg(QString::fromLatin1(outputFileExtension(protocol == OpenPGP ? Class::OpenPGP | Class::Ascii | Class::Certificate
                                                                                            : Class::CMS | Class::Binary | Class::ExportedPSM,
                                                                        usePGPFileExt)));

    m_filename = FileDialog::getSaveFileNameEx(parent ? parent : d->parentWidgetOrView(),
                                               i18n("Backup Secret Key"),
                                               QStringLiteral("imp"),
                                               proposedFileName,
                                               protocol == GpgME::OpenPGP ? i18n("Secret Key Files") + QLatin1String(" (*.asc *.gpg *.pgp)")
                                                                          : i18n("Secret Key Files") + QLatin1String(" (*.p12)"));

    m_armor = m_filename.endsWith(QLatin1String(".asc"));

    return !m_filename.isEmpty();
}

QStringList ExportSecretKeyCommand::arguments() const
{
    const Key key = d->key();
    QStringList result;

    if (key.protocol() == OpenPGP) {
        result << gpgPath() << QStringLiteral("--batch");
    } else {
        result << gpgSmPath();
    }

    result << QStringLiteral("--yes") << QStringLiteral("--output") << QStringLiteral("-");

    if (m_armor) {
        result << QStringLiteral("--armor");
    }

    if (key.protocol() == CMS) {
        result << QStringLiteral("--p12-charset") << QStringLiteral("utf-8");
    }

    if (key.protocol() == OpenPGP) {
        result << QStringLiteral("--export-secret-key");
    } else {
        result << QStringLiteral("--export-secret-key-p12");
    }

    result << QLatin1String(key.primaryFingerprint());

    return result;
}

QString ExportSecretKeyCommand::errorCaption() const
{
    return i18nc("@title:window", "Secret Key Export Error");
}

QString ExportSecretKeyCommand::successCaption() const
{
    return i18nc("@title:window", "Secret Key Export Finished");
}

QString ExportSecretKeyCommand::crashExitMessage(const QStringList &args) const
{
    return xi18nc("@info",
                  "<para>The GPG or GpgSM process that tried to export the secret key "
                  "ended prematurely because of an unexpected error.</para>"
                  "<para>Please check the output of <icode>%1</icode> for details.</para>",
                  args.join(QLatin1Char(' ')));
}

QString ExportSecretKeyCommand::errorExitMessage(const QStringList &args) const
{
    return xi18nc("@info",
                  "<para>An error occurred while trying to export the secret key.</para> "
                  "<para>The output from <command>%1</command> was: <message>%2</message></para>",
                  args[0],
                  errorString());
}

QString ExportSecretKeyCommand::successMessage(const QStringList &) const
{
    if (mHasError) {
        return QString();
    }
    return i18nc("@info", "Secret key successfully exported.");
}

void ExportSecretKeyCommand::postSuccessHook(QWidget *)
{
    Q_ASSERT(process());
    const auto data = process()->readAllStandardOutput();
    if (!data.size()) {
        d->error(i18nc("@info", "Possibly bad passphrase given."), errorCaption());
        mHasError = true;
        return;
    }
    QFile file(m_filename);
    /* The filedialog already asked for replace ok. */
    file.open(QIODevice::ReadWrite | QIODevice::Truncate);
    if (file.write(data) != data.size()) {
        d->error(i18nc("@info", "Failed to write data."), errorCaption());
        mHasError = true;
    }
    file.close();
}

}
