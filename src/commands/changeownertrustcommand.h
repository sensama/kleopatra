/* -*- mode: c++; c-basic-offset:4 -*-
    commands/changeownertrustcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMMANDS_CHANGEOWNERTRUSTCOMMAND_H__
#define __KLEOPATRA_COMMMANDS_CHANGEOWNERTRUSTCOMMAND_H__

#include <commands/command.h>

namespace Kleo
{
namespace Commands
{

class ChangeOwnerTrustCommand : public Command
{
    Q_OBJECT
public:
    explicit ChangeOwnerTrustCommand(QAbstractItemView *view, KeyListController *parent);
    explicit ChangeOwnerTrustCommand(KeyListController *parent);
    explicit ChangeOwnerTrustCommand(const GpgME::Key &key);
    ~ChangeOwnerTrustCommand() override;

    /* reimp */ static Restrictions restrictions()
    {
        return OnlyOneKey | MustBeOpenPGP | MayOnlyBeSecretKeyIfOwnerTrustIsNotYetUltimate;
    }

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotResult(GpgME::Error))
    Q_PRIVATE_SLOT(d_func(), void slotDialogAccepted())
    Q_PRIVATE_SLOT(d_func(), void slotDialogRejected())
};

}
}

#endif // __KLEOPATRA_COMMMANDS_CHANGEOWNERTRUSTCOMMAND_H__
