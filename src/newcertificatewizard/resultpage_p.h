/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/resultpage_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "wizardpage_p.h"

namespace GpgME
{
class Key;
}
namespace Kleo
{
class ExportCertificateCommand;
}
namespace Kleo::NewCertificateUi
{
class Ui_ResultPage;

class ResultPage : public WizardPage
{
    Q_OBJECT
public:
    explicit ResultPage(QWidget *p = nullptr);
    ~ResultPage() override;

    void initializePage() override;
    bool isError() const;
    bool isComplete() const override;

private:
    GpgME::Key key() const;

private Q_SLOTS:
    void slotSaveRequestToFile();
    void slotSendRequestByEMail();
    void slotSendCertificateByEMail();
    void slotSendCertificateByEMailContinuation();
    void invokeMailer(const QString &to, const QString &subject, const QString &body, const QString &attachment);
    void slotUploadCertificateToDirectoryServer();
    void slotBackupCertificate();
    void slotCreateRevocationRequest();
    void slotCreateSigningCertificate();
    void slotCreateEncryptionCertificate();

private:
    void toggleSignEncryptAndRestart();

private:
    std::unique_ptr<Kleo::NewCertificateUi::Ui_ResultPage> ui;
    bool initialized : 1;
    bool successfullyCreatedSigningCertificate : 1;
    bool successfullyCreatedEncryptionCertificate : 1;
    QPointer<Kleo::ExportCertificateCommand> exportCertificateCommand;
};

}
