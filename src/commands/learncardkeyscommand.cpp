/* -*- mode: c++; c-basic-offset:4 -*-
    commands/learncardkeyscommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "learncardkeyscommand.h"

#include "command_p.h"

#include <smartcard/readerstatus.h>

#include <Libkleo/GnuPG>

#include <gpgme++/key.h>

#include <KLocalizedString>

#include <QProgressDialog>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

LearnCardKeysCommand::LearnCardKeysCommand(GpgME::Protocol proto)
    : GnuPGProcessCommand(nullptr), m_protocol(proto)
{
    setIgnoresSuccessOrFailure(true);
    setShowsOutputWindow(true);
    connect(this, &Command::finished,
            SmartCard::ReaderStatus::mutableInstance(), &SmartCard::ReaderStatus::updateStatus);
}

LearnCardKeysCommand::~LearnCardKeysCommand() {}

Protocol LearnCardKeysCommand::protocol() const
{
    return m_protocol;
}

QStringList LearnCardKeysCommand::arguments() const
{
    if (protocol() == OpenPGP) {
        return QStringList() << gpgPath() << QStringLiteral("--batch") << QStringLiteral("--card-status") << QStringLiteral("-v");
    } else {
        return QStringList() << gpgSmPath() << QStringLiteral("--learn-card") << QStringLiteral("-v");
    }
}

QString LearnCardKeysCommand::errorCaption() const
{
    return i18nc("@title:window", "Error Learning SmartCard");
}

QString LearnCardKeysCommand::successCaption() const
{
    return i18nc("@title:window", "Finished Learning SmartCard");
}

QString LearnCardKeysCommand::crashExitMessage(const QStringList &args) const
{
    return xi18nc("@info",
                  "<para>The GPG or GpgSM process that tried to learn the smart card "
                  "ended prematurely because of an unexpected error.</para>"
                  "<para>Please check the output of <icode>%1</icode> for details.</para>", args.join(QLatin1Char(' ')));
}

QString LearnCardKeysCommand::errorExitMessage(const QStringList &) const
{
    // unused, since we setIgnoresSuccessOrFailure(true)
    return QString();
}

QString LearnCardKeysCommand::successMessage(const QStringList &) const
{
    // unused, since we setIgnoresSuccessOrFailure(true)
    return QString();
}

void LearnCardKeysCommand::doStart()
{
    GnuPGProcessCommand::doStart();

    auto detailsDlg = dialog();
    if (detailsDlg) {
        detailsDlg->hide();
    }

    const auto dlg = new QProgressDialog(i18n("Loading certificates... (this can take a while)"),
            i18n("Show Details"), 0, 0, d->parentWidgetOrView());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModal(true);
    connect(dlg, &QProgressDialog::canceled, this, [detailsDlg] () {
            if (detailsDlg) {
                detailsDlg->show();
            }
        });
    connect(this, &LearnCardKeysCommand::finished, this, [dlg] () {
            dlg->accept();
        });
    dlg->show();
}
