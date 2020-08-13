/* -*- mode: c++; c-basic-offset:4 -*-
    commands/signcertificatecommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_CERTIFYCERTIFICATECOMMAND_H__
#define __KLEOPATRA_COMMANDS_CERTIFYCERTIFICATECOMMAND_H__

#include <commands/command.h>

namespace GpgME
{
class UserID;
}

namespace Kleo
{
namespace Commands
{

class CertifyCertificateCommand : public Command
{
    Q_OBJECT
public:
    explicit CertifyCertificateCommand(QAbstractItemView *view, KeyListController *parent);
    explicit CertifyCertificateCommand(KeyListController *parent);
    explicit CertifyCertificateCommand(const GpgME::Key &key);
    explicit CertifyCertificateCommand(const GpgME::UserID &uid);
    explicit CertifyCertificateCommand(const std::vector<GpgME::UserID> &uids);
    ~CertifyCertificateCommand() override;

    /* reimp */ static Restrictions restrictions()
    {
        return OnlyOneKey | MustBeOpenPGP;
    }

    void setCertificationExportable(bool on);
    void setCertificationRevocable(bool on);

    void setCertifyingKey(const GpgME::Key &key);

    void setUserID(const GpgME::UserID &uid);
    void setUserIDs(const std::vector<GpgME::UserID> &uids);

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotResult(GpgME::Error))
    Q_PRIVATE_SLOT(d_func(), void slotDialogRejected())
    Q_PRIVATE_SLOT(d_func(), void slotCertificationPrepared())
};

}
}

#endif // __KLEOPATRA_COMMANDS_SIGNCERTIFICATECOMMAND_H__
