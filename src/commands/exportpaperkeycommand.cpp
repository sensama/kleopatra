/* -*- mode: c++; c-basic-offset:4 -*-
    commands/exportpaperkeycommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "exportpaperkeycommand.h"

#include <Libkleo/GnuPG>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include <KMessageBox>

#include <QFontDatabase>
#include <QPrintDialog>
#include <QPrinter>
#include <QTextDocument>

#include "command_p.h"
#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

ExportPaperKeyCommand::ExportPaperKeyCommand(QAbstractItemView *v, KeyListController *c)
    : GnuPGProcessCommand(v, c)
    , mParent(v)
{
    connect(&mPkProc, &QProcess::finished, this, &ExportPaperKeyCommand::pkProcFinished);
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
    result << QLatin1StringView(key.primaryFingerprint());

    return result;
}

bool ExportPaperKeyCommand::preStartHook(QWidget *parent) const
{
    if (paperKeyInstallPath().isNull()) {
        KMessageBox::error(parent,
                           xi18nc("@info",
                                  "<para><application>Kleopatra</application> uses "
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
                  args[0],
                  errorString());
}

#include "moc_exportpaperkeycommand.cpp"
