/* -*- mode: c++; c-basic-offset:4 -*-
    core/selectcertificatecommand.h

    This file is part of KleopatraClient, the Kleopatra interface library
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <libkleopatraclient/core/command.h>

namespace KleopatraClientCopy
{

class KLEOPATRACLIENTCORE_EXPORT SelectCertificateCommand : public Command
{
    Q_OBJECT
public:
    explicit SelectCertificateCommand(QObject *parent = nullptr);
    ~SelectCertificateCommand();

    // Inputs

    void setMultipleCertificatesAllowed(bool allow);
    bool multipleCertificatesAllowed() const;

    void setOnlySigningCertificatesAllowed(bool allow);
    bool onlySigningCertificatesAllowed() const;

    void setOnlyEncryptionCertificatesAllowed(bool allow);
    bool onlyEncryptionCertificatesAllowed() const;

    void setOnlyOpenPGPCertificatesAllowed(bool allow);
    bool onlyOpenPGPCertificatesAllowed() const;

    void setOnlyX509CertificatesAllowed(bool allow);
    bool onlyX509CertificatesAllowed() const;

    void setOnlySecretKeysAllowed(bool allow);
    bool onlySecretKeysAllowed() const;

    // Input/Outputs

    void setSelectedCertificates(const QStringList &certs);
    QStringList selectedCertificates() const;

    void setSelectedCertificate(const QString &cert);
    QString selectedCertificate() const;

};

}

