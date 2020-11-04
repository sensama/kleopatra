/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/revokecertificationdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_DIALOGS_REVOKECERTIFICATIONDIALOG_H__
#define __KLEOPATRA_DIALOGS_REVOKECERTIFICATIONDIALOG_H__

#include <QDialog>

namespace GpgME
{
class Key;
class UserID;
}

namespace Kleo
{

class RevokeCertificationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RevokeCertificationDialog(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~RevokeCertificationDialog() override;

    void setCertificateToRevoke(const GpgME::Key &key);

    void setSelectedUserIDs(const std::vector<GpgME::UserID> &uids);
    std::vector<GpgME::UserID> selectedUserIDs() const;

    GpgME::Key selectedCertificationKey() const;

    bool sendToServer() const;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

} // namespace Kleo

#endif /* __KLEOPATRA_DIALOGS_REVOKECERTIFICATIONDIALOG_H__ */
