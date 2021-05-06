/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/signcertificatedialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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

    bool trustSignatureSelected() const;
    QString trustSignatureDomain() const;

    bool nonRevocableCertificationSelected() const;

    void setSelectedUserIDs(const std::vector<GpgME::UserID> &uids);
    std::vector<unsigned int> selectedUserIDs() const;

    GpgME::Key selectedSecretKey() const;

    bool sendToServer() const;

    unsigned int selectedCheckLevel() const;

    void setCertificateToCertify(const GpgME::Key &key);

    QString tags() const;

public Q_SLOTS:
    void accept() override;

private:
    CertifyWidget *mCertWidget;
};

}


