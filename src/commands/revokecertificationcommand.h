/* -*- mode: c++; c-basic-offset:4 -*-
    commands/revokecertificationcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_REVOKECERTIFICATIONCOMMAND_H__
#define __KLEOPATRA_COMMANDS_REVOKECERTIFICATIONCOMMAND_H__

#include <commands/command.h>

#include <gpgme++/key.h>

namespace Kleo
{
namespace Commands
{

class RevokeCertificationCommand : public Command
{
    Q_OBJECT
public:
    explicit RevokeCertificationCommand(QAbstractItemView *view, KeyListController *parent);
    explicit RevokeCertificationCommand(const GpgME::UserID &uid);
    explicit RevokeCertificationCommand(const GpgME::UserID::Signature &signature);
    ~RevokeCertificationCommand() override;

    /* reimp */ static Restrictions restrictions()
    {
        return OnlyOneKey | MustBeOpenPGP;
    }

    static bool isSupported();

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

} // namespace Commands
} // namespace Kleo

#endif // __KLEOPATRA_COMMANDS_REVOKECERTIFICATIONCOMMAND_H__
