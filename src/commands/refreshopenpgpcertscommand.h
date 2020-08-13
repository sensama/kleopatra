/* -*- mode: c++; c-basic-offset:4 -*-
    commands/refreshopenpgpcertscommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMMANDS_REFRESHOPENPGPCERTSCOMMAND_H__
#define __KLEOPATRA_COMMMANDS_REFRESHOPENPGPCERTSCOMMAND_H__

#include <commands/gnupgprocesscommand.h>

namespace Kleo
{
namespace Commands
{

class RefreshOpenPGPCertsCommand : public GnuPGProcessCommand
{
    Q_OBJECT
public:
    explicit RefreshOpenPGPCertsCommand(QAbstractItemView *view, KeyListController *parent);
    explicit RefreshOpenPGPCertsCommand(KeyListController *parent);
    ~RefreshOpenPGPCertsCommand() override;

private:
    bool preStartHook(QWidget *) const override;

    QStringList arguments() const override;

    QString errorCaption() const override;
    QString successCaption() const override;

    QString crashExitMessage(const QStringList &) const override;
    QString errorExitMessage(const QStringList &) const override;
    QString successMessage(const QStringList &) const override;
};

}
}

#endif // __KLEOPATRA_COMMMANDS_REFRESHOPENPGPCERTSCOMMAND_H__
