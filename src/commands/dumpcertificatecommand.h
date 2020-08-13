/* -*- mode: c++; c-basic-offset:4 -*-
    commands/dumpcertificatecommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMMANDS_DUMPCERTIFICATECOMMAND_H__
#define __KLEOPATRA_COMMMANDS_DUMPCERTIFICATECOMMAND_H__

#include <commands/command.h>

namespace Kleo
{
namespace Commands
{

class DumpCertificateCommand : public Command
{
    Q_OBJECT
public:
    explicit DumpCertificateCommand(QAbstractItemView *view, KeyListController *parent);
    explicit DumpCertificateCommand(KeyListController *parent);
    explicit DumpCertificateCommand(const GpgME::Key &key);
    ~DumpCertificateCommand() override;

    static Restrictions restrictions()
    {
        return OnlyOneKey | MustBeCMS;
    }

    void setUseDialog(bool on);
    bool useDialog() const;

    QStringList output() const;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotProcessFinished(int, QProcess::ExitStatus))
    Q_PRIVATE_SLOT(d_func(), void slotProcessReadyReadStandardOutput())
    Q_PRIVATE_SLOT(d_func(), void slotProcessReadyReadStandardError())
    Q_PRIVATE_SLOT(d_func(), void slotUpdateRequested())
    Q_PRIVATE_SLOT(d_func(), void slotDialogDestroyed())
};

}
}

#endif // __KLEOPATRA_COMMMANDS_DUMPCERTIFICATECOMMAND_H__
