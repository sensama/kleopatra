/* -*- mode: c++; c-basic-offset:4 -*-
    gui/certificaterequester.h

    This file is part of KleopatraClient, the Kleopatra interface library
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "kleopatraclientgui_export.h"

#include <QWidget>

namespace KleopatraClientCopy
{
namespace Gui
{

class KLEOPATRACLIENTGUI_EXPORT CertificateRequester : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool multipleCertificatesAllowed READ multipleCertificatesAllowed WRITE setMultipleCertificatesAllowed)
    Q_PROPERTY(bool onlySigningCertificatesAllowed READ onlySigningCertificatesAllowed WRITE setOnlySigningCertificatesAllowed)
    Q_PROPERTY(bool onlyEncryptionCertificatesAllowed READ onlyEncryptionCertificatesAllowed WRITE setOnlyEncryptionCertificatesAllowed)
    Q_PROPERTY(bool onlyOpenPGPCertificatesAllowed READ onlyOpenPGPCertificatesAllowed WRITE setOnlyOpenPGPCertificatesAllowed)
    Q_PROPERTY(bool onlyX509CertificatesAllowed READ onlyX509CertificatesAllowed WRITE setOnlyX509CertificatesAllowed)
    Q_PROPERTY(bool onlySecretKeysAllowed READ onlySecretKeysAllowed WRITE setOnlySecretKeysAllowed)
    Q_PROPERTY(QStringList selectedCertificates READ selectedCertificates WRITE setSelectedCertificates)
public:
    explicit CertificateRequester(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~CertificateRequester() override;

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

    void setSelectedCertificates(const QStringList &certs);
    QStringList selectedCertificates() const;

    void setSelectedCertificate(const QString &cert);
    QString selectedCertificate() const;

Q_SIGNALS:
    void selectedCertificatesChanged(const QStringList &certs);

private:
    class Private;
    Private *d;
    Q_PRIVATE_SLOT(d, void slotButtonClicked())
    Q_PRIVATE_SLOT(d, void slotCommandFinished())
};

}
}
