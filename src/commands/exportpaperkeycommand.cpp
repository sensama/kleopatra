/* -*- mode: c++; c-basic-offset:4 -*-
    commands/exportpaperkeycommand.cpp

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

#include "exportpaperkeycommand.h"

#include <utils/gnupg-helper.h>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include <KMessageBox>

#include <QProcess>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>
#include <QFontDatabase>

#include "kleopatra_debug.h"
#include "command_p.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

ExportPaperKeyCommand::ExportPaperKeyCommand(QAbstractItemView *v, KeyListController *c) :
    GnuPGProcessCommand(v, c),
    mParent(v)
{
    connect(&mPkProc, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &ExportPaperKeyCommand::pkProcFinished);
    mPkProc.setProgram(paperKeyInstallPath());
    mPkProc.setArguments(QStringList() << QStringLiteral("--output-type=base16"));

    process()->setStandardOutputProcess(&mPkProc);
    qCDebug(KLEOPATRA_LOG) << "Starting PaperKey process.";
    mPkProc.start();
    setAutoDelete(false);
}

QStringList ExportPaperKeyCommand::arguments() const
{
    const Key key = d->key();
    QStringList result;

    result << gpgPath() << QStringLiteral("--batch");
    result << QStringLiteral("--export-secret-key");
    result << QLatin1String(key.primaryFingerprint());

    return result;
}

bool ExportPaperKeyCommand::preStartHook(QWidget *parent) const
{
    if (paperKeyInstallPath().isNull()) {
        KMessageBox::sorry(parent, xi18nc("@info", "<para><application>Kleopatra</application> uses "
                                                   "<application>PaperKey</application> to create a minimized and"
                                                   " printable version of your secret key.</para>"
                                                   "<para>Please make sure it is installed.</para>"),
                           i18nc("@title", "Failed to find PaperKey executable."));
        return false;
    }
    return true;
}

void ExportPaperKeyCommand::pkProcFinished(int code, QProcess::ExitStatus status)
{
    qCDebug(KLEOPATRA_LOG) << "Paperkey export finished: " << code << "status: " << status;

    if (status == QProcess::CrashExit || code) {
        qCDebug(KLEOPATRA_LOG) << "Aborting because paperkey failed";
        deleteLater();
        return;
    }

    QPrinter printer;

    const Key key = d->key();
    printer.setDocName(QStringLiteral("0x%1-sec").arg(QString::fromLatin1(key.shortKeyID())));
    QPrintDialog printDialog(&printer, mParent);
    printDialog.setWindowTitle(i18nc("@title:window", "Print Secret Key"));

    if (printDialog.exec() != QDialog::Accepted) {
        qCDebug(KLEOPATRA_LOG) << "Printing aborted.";
        deleteLater();
        return;
    }

    QTextDocument doc(QString::fromLatin1(mPkProc.readAllStandardOutput()));
    doc.setDefaultFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    doc.print(&printer);

    deleteLater();
}

QString ExportPaperKeyCommand::errorCaption() const
{
    return i18nc("@title:window", "Error printing secret key");
}

QString ExportPaperKeyCommand::crashExitMessage(const QStringList &args) const
{
    return xi18nc("@info",
                  "<para>The GPG process that tried to export the secret key "
                  "ended prematurely because of an unexpected error.</para>"
                  "<para>Please check the output of <icode>%1</icode> for details.</para>",
                  args.join(QLatin1Char(' ')));
}

QString ExportPaperKeyCommand::errorExitMessage(const QStringList &args) const
{
    return xi18nc("@info",
                  "<para>An error occurred while trying to export the secret key.</para> "
                  "<para>The output from <command>%1</command> was: <message>%2</message></para>",
                  args[0], errorString());
}
