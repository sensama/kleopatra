/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/keycreationpage_p.h

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
class KeyGenerationResult;
}
namespace QGpgME
{
class KeyGenerationJob;
}
namespace Kleo::NewCertificateUi
{
class Ui_KeyCreationPage;
}

class KeyCreationPage : public Kleo::NewCertificateUi::WizardPage
{
    Q_OBJECT
public:
    explicit KeyCreationPage(QWidget *p = nullptr);
    ~KeyCreationPage() override;

    bool isComplete() const override;

    void initializePage() override;

private:
    void startJob();
    QStringList keyUsages() const;
    QStringList subkeyUsages() const;
    QString createGnupgKeyParms() const;

private Q_SLOTS:
    void slotResult(const GpgME::KeyGenerationResult &result, const QByteArray &request, const QString &auditLog);

private:
    class EmptyPassphraseProvider;
    std::unique_ptr<EmptyPassphraseProvider> mEmptyPWProvider;
    std::unique_ptr<Kleo::NewCertificateUi::Ui_KeyCreationPage> ui;
    QPointer<QGpgME::KeyGenerationJob> job;
};
