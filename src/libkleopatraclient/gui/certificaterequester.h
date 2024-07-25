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
    Q_PROPERTY(bool multipleCertificatesAllowed READ multipleCertificatesAllowed WRITE setMultipleCertificatesAllowed NOTIFY multipleCertificatesAllowedChanged)
    Q_PROPERTY(bool onlySigningCertificatesAllowed READ onlySigningCertificatesAllowed WRITE setOnlySigningCertificatesAllowed NOTIFY
                   onlySigningCertificatesAllowedChanged)
    Q_PROPERTY(bool onlyEncryptionCertificatesAllowed READ onlyEncryptionCertificatesAllowed WRITE setOnlyEncryptionCertificatesAllowed NOTIFY
                   onlyEncryptionCertificatesAllowedChanged)
    Q_PROPERTY(bool onlyOpenPGPCertificatesAllowed READ onlyOpenPGPCertificatesAllowed WRITE setOnlyOpenPGPCertificatesAllowed NOTIFY
                   onlyOpenPGPCertificatesAllowedChanged)
    Q_PROPERTY(bool onlyX509CertificatesAllowed READ onlyX509CertificatesAllowed WRITE setOnlyX509CertificatesAllowed NOTIFY onlyX509CertificatesAllowedChanged)
    Q_PROPERTY(bool onlySecretKeysAllowed READ onlySecretKeysAllowed WRITE setOnlySecretKeysAllowed NOTIFY onlySecretKeysAllowedChanged)
    Q_PROPERTY(QStringList selectedCertificates READ selectedCertificates WRITE setSelectedCertificates NOTIFY selectedCertificatesChanged)
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
    void multipleCertificatesAllowedChanged(bool multipleCertificatesAllowed);
    void onlySigningCertificatesAllowedChanged(bool onlySigningCertificatesAllowed);
    void onlyEncryptionCertificatesAllowedChanged(bool onlyEncryptionCertificatesAllowed);
    void onlyOpenPGPCertificatesAllowedChanged(bool onlyOpenPGPCertificatesAllowed);
    void onlyX509CertificatesAllowedChanged(bool onlyX509CertificatesAllowed);
    void onlySecretKeysAllowedChanged(bool onlySecretKeysAllowed);

    void selectedCertificatesChanged(const QStringList &certs);

private:
    class Private;
    Private *d;
    Q_PRIVATE_SLOT(d, void slotButtonClicked())
    Q_PRIVATE_SLOT(d, void slotCommandFinished())
};

}
}
