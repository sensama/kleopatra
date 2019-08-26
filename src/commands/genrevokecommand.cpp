/* -*- mode: c++; c-basic-offset:4 -*-
    commands/genrevokecommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2017 by Bundesamt für Sicherheit in der Informationstechnik
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301  USA

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

#include <config-kleopatra.h>

#include "genrevokecommand.h"

#include <utils/gnupg-helper.h>

#include <Libkleo/Formatting>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include <KMessageBox>

#include <QProcess>
#include <QTextStream>
#include <QFile>
#include <QFileDialog>

#include "kleopatra_debug.h"
#include "command_p.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

GenRevokeCommand::GenRevokeCommand(QAbstractItemView *v, KeyListController *c) :
    GnuPGProcessCommand(v, c)
{
}

GenRevokeCommand::GenRevokeCommand(KeyListController *c)
    : GnuPGProcessCommand(c)
{
}

GenRevokeCommand::GenRevokeCommand(const Key &key)
    : GnuPGProcessCommand(key)
{
}

// Fixup the revocation certificate similar to GnuPG
void GenRevokeCommand::postSuccessHook(QWidget *parentWidget)
{
    QFile f(mOutputFileName);

    if (!f.open(QIODevice::ReadOnly)) {
        // Should never happen because in this case we would not have had a success.
        KMessageBox::error(parentWidget, errorCaption(),
                           QStringLiteral("Failed to access the created output file."));
        return;
    }
    const QString revCert = QString::fromLocal8Bit(f.readAll());
    f.close();

    if (!f.open(QIODevice::WriteOnly)) {
        KMessageBox::error(parentWidget, errorCaption(),
                           QStringLiteral("Failed to write to the created output file."));
        return;
    }

    QTextStream s(&f);

    s << i18n("This is a revocation certificate for the OpenPGP key:")
      << "\n\n             " << Formatting::prettyNameAndEMail(d->key())
      <<   "\n Fingerprint: " << d->key().primaryFingerprint() << "\n\n"
      << i18n("A revocation certificate is a kind of \"kill switch\" to publicly\n"
              "declare that a key shall not anymore be used.  It is not possible\n"
              "to retract such a revocation certificate once it has been published.")
      << "\n\n"
      << i18n("Use it to revoke this key in case of a compromise or loss of\n"
              "the secret key.")
      << "\n\n"
      << i18n("To avoid an accidental use of this file, a colon has been inserted\n"
              "before the 5 dashes below.  Remove this colon with a text editor\n"
              "before importing and publishing this revocation certificate.")
      << "\n\n:"
      << revCert;
    s.flush();
    qCDebug(KLEOPATRA_LOG) << "revocation certificate stored as:" << mOutputFileName;

    f.close();
    KMessageBox::information(d->parentWidgetOrView(),
                             i18nc("@info", "Certificate successfully created.<br><br>"
                                  "Note:<br>To prevent accidental import of the revocation<br>"
                                  "it is required to manually edit the certificate<br>"
                                  "before it can be imported."),
                             i18n("Revocation certificate created"));
}

/* Well not much to do with GnuPGProcessCommand anymore I guess.. */
void GenRevokeCommand::doStart()
{
    while (mOutputFileName.isEmpty()) {
        mOutputFileName = QFileDialog::getSaveFileName(d->parentWidgetOrView(), i18n("Generate revocation certificate"),
                                                       QString(),
                                                       QStringLiteral("%1 (*.rev)").arg(i18n("Revocation Certificates ")));
        if (mOutputFileName.isEmpty()) {
            d->finished();
            return;
        }
        if (!mOutputFileName.endsWith(QLatin1String(".rev"))) {
            mOutputFileName += QLatin1String(".rev");
        }
        if (QFileInfo(mOutputFileName).exists()) {
            auto sel = KMessageBox::questionYesNo(d->parentWidgetOrView(), i18n("The file <b>%1</b> already exists.\n"
                                                  "Overwrite?", mOutputFileName),
                                                  i18n("Overwrite Existing File?"));
            if (sel == KMessageBox::No) {
                mOutputFileName = QString();
            }
        }
    }


    auto proc = process();
    // We do custom io
    disconnect(proc, SIGNAL(readyReadStandardError()),
               this, SLOT(slotProcessReadyReadStandardError()));
    proc->setReadChannel(QProcess::StandardOutput);

    GnuPGProcessCommand::doStart();


    connect(proc, &QProcess::readyReadStandardOutput,
            this, [proc] () {
        while (proc->canReadLine()) {
            const QString line = QString::fromUtf8(proc->readLine()).trimmed();
            // Command-fd is a stable interface, while this is all kind of hacky we
            // are on a deadline :-/
            if (line == QLatin1String("[GNUPG:] GET_BOOL gen_revoke.okay")) {
                proc->write("y\n");
            } else if (line == QLatin1String("[GNUPG:] GET_LINE ask_revocation_reason.code")) {
                proc->write("0\n");
            } else if (line == QLatin1String("[GNUPG:] GET_LINE ask_revocation_reason.text")) {
                proc->write("\n");
            } else if (line == QLatin1String("[GNUPG:] GET_BOOL openfile.overwrite.okay")) {
                // We asked before
                proc->write("y\n");
            } else if (line == QLatin1String("[GNUPG:] GET_BOOL ask_revocation_reason.okay")) {
                proc->write("y\n");
            }
        }
    });
}

QStringList GenRevokeCommand::arguments() const
{
    const Key key = d->key();
    QStringList result;

    result << gpgPath() << QStringLiteral("--command-fd") << QStringLiteral("0") << QStringLiteral("--status-fd") << QStringLiteral("1")
           << QStringLiteral("-o") << mOutputFileName
           << QStringLiteral("--gen-revoke")
           << QLatin1String(key.primaryFingerprint());

    return result;
}


QString GenRevokeCommand::errorCaption() const
{
    return i18nc("@title:window", "Error creating revocation certificate");
}

QString GenRevokeCommand::crashExitMessage(const QStringList &) const
{
    // We show a success message so a failure is either the user aborted
    // or a bug.
    qCDebug(KLEOPATRA_LOG) << "Crash exit of GenRevokeCommand";
    return QString();
}

QString GenRevokeCommand::errorExitMessage(const QStringList &) const
{
    // We show a success message so a failure is either the user aborted
    // or a bug.
    qCDebug(KLEOPATRA_LOG) << "Error exit of GenRevokeCommand";
    return QString();
}
