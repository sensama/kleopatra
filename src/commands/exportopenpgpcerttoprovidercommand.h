/* -*- mode: c++; c-basic-offset:4 -*-
    commands/exportopenpgpcertstoservercommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2019 Felix Tiede

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <commands/gnupgprocesscommand.h>

#include <kidentitymanagement/identitymanager.h>

#include <QtCore/QTemporaryFile>

#include <gpgme++/key.h>

namespace Kleo
{
namespace Commands
{

class ExportOpenPGPCertToProviderCommand : public GnuPGProcessCommand
{
    Q_OBJECT
public:
    explicit ExportOpenPGPCertToProviderCommand(QAbstractItemView *view, KeyListController *parent);
    explicit ExportOpenPGPCertToProviderCommand(const GpgME::UserID &uid);

    ~ExportOpenPGPCertToProviderCommand() override;

    static Restrictions restrictions()
    {
        return OnlyOneKey | NeedSecretKey | MustBeOpenPGP;
    }

private:
    bool preStartHook(QWidget *) const override;
    void postSuccessHook(QWidget *) override;

    QStringList arguments() const override;

    QString errorCaption() const override;
    QString successCaption() const override;

    QString crashExitMessage(const QStringList &) const override;
    QString errorExitMessage(const QStringList &) const override;
    QString successMessage(const QStringList &) const override;

    GpgME::UserID uid;

    QTemporaryFile wksMail;
    static const KIdentityManagement::IdentityManager *mailIdManager;
};

}
}
