/* -*- mode: c++; c-basic-offset:4 -*-
    commands/exportpaperkeycommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <commands/gnupgprocesscommand.h>

#include <QProcess>
#include <QString>

class QWidget;

namespace Kleo
{
namespace Commands
{

class ExportPaperKeyCommand : public GnuPGProcessCommand
{
    Q_OBJECT

public:
    explicit ExportPaperKeyCommand(QAbstractItemView *view, KeyListController *parent);

    static Restrictions restrictions()
    {
        return OnlyOneKey | NeedSecretKeyData | MustBeOpenPGP;
    }

protected Q_SLOTS:
    void pkProcFinished(int code, QProcess::ExitStatus status);

private:
    QStringList arguments() const override;
    bool preStartHook(QWidget *parentWidget) const override;

    QString errorCaption() const override;

    QString crashExitMessage(const QStringList &) const override;
    QString errorExitMessage(const QStringList &) const override;

private:
    QWidget *const mParent;
    QProcess mPkProc;
};

}
}
