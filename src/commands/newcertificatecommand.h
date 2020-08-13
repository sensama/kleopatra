/* -*- mode: c++; c-basic-offset:4 -*-
    commands/newcertificatecommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_NEWCERTIFICATECOMMAND_H__
#define __KLEOPATRA_COMMANDS_NEWCERTIFICATECOMMAND_H__

#include <commands/command.h>

#include <gpgme++/global.h>

namespace Kleo
{
namespace Commands
{

class NewCertificateCommand : public Command
{
    Q_OBJECT
public:
    explicit NewCertificateCommand(QAbstractItemView *view, KeyListController *parent);
    explicit NewCertificateCommand(KeyListController *parent);
    explicit NewCertificateCommand();
    ~NewCertificateCommand() override;

    void setProtocol(GpgME::Protocol proto);
    GpgME::Protocol protocol() const;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotDialogRejected())
    Q_PRIVATE_SLOT(d_func(), void slotDialogAccepted())
};

}
}

#endif // __KLEOPATRA_COMMANDS_NEWCERTIFICATECOMMAND_H__
