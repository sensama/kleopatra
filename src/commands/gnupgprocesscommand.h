/* -*- mode: c++; c-basic-offset:4 -*-
    commands/gnupgprocesscommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <commands/command.h>

#include <QStringList>
class QString;
class QProcess;

namespace Kleo
{
namespace Commands
{

class GnuPGProcessCommand : public Command
{
    Q_OBJECT
protected:
    explicit GnuPGProcessCommand(QAbstractItemView *view, KeyListController *parent);
    explicit GnuPGProcessCommand(KeyListController *parent);
    explicit GnuPGProcessCommand(const GpgME::Key &key);
    ~GnuPGProcessCommand() override;

public:
    QDialog *dialog() const;

private:
    virtual bool preStartHook(QWidget *parentWidget) const;

    virtual QStringList arguments() const = 0;

    virtual QString errorCaption() const = 0;
    virtual QString successCaption() const;

    virtual QString crashExitMessage(const QStringList &args) const = 0;
    virtual QString errorExitMessage(const QStringList &args) const = 0;
    virtual QString successMessage(const QStringList &args) const;

    virtual void postSuccessHook(QWidget *parentWidget);

protected:
    QString errorString() const;
    void setIgnoresSuccessOrFailure(bool ignore);
    bool ignoresSuccessOrFailure() const;
    void setShowsOutputWindow(bool show);
    bool showsOutputWindow() const;

    QProcess *process();

    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotProcessFinished(int, QProcess::ExitStatus))
    Q_PRIVATE_SLOT(d_func(), void slotProcessReadyReadStandardError())
};

}
}

