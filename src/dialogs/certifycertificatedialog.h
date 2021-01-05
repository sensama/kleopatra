/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/signcertificatedialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_DIALOGS_CERTIFYCERTIFICATEDIALOG_H__
#define __KLEOPATRA_DIALOGS_CERTIFYCERTIFICATEDIALOG_H__

#include <QWizard>

#include <QGpgME/SignKeyJob>

#include <gpgme++/key.h>

#include <utils/pimpl_ptr.h>

namespace Kleo
{

class CertifyWidget;

class CertifyCertificateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CertifyCertificateDialog(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~CertifyCertificateDialog();

    bool exportableCertificationSelected() const;

    bool trustCertificationSelected() const;

    bool nonRevocableCertificationSelected() const;

    void setSelectedUserIDs(const std::vector<GpgME::UserID> &uids);
    std::vector<unsigned int> selectedUserIDs() const;

    void setCertificatesWithSecretKeys(const std::vector<GpgME::Key> &keys);
    GpgME::Key selectedSecretKey() const;

    bool sendToServer() const;

    unsigned int selectedCheckLevel() const;

    void setCertificateToCertify(const GpgME::Key &key);

    QString tags() const;

private:
    CertifyWidget *mCertWidget;
};

}

#endif /* __KLEOPATRA_DIALOGS_CERTIFYCERTIFICATEDIALOG_H__ */

