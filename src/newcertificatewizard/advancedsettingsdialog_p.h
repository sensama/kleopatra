/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/advancedsettingsdialog_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/metatypes_for_gpgmepp_key.h"

#include <QDialog>

#include <gpgme++/key.h>

#include <memory>

class AdvancedSettingsDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QStringList additionalUserIDs READ additionalUserIDs WRITE setAdditionalUserIDs)
    Q_PROPERTY(QStringList additionalEMailAddresses READ additionalEMailAddresses WRITE setAdditionalEMailAddresses)
    Q_PROPERTY(QStringList dnsNames READ dnsNames WRITE setDnsNames)
    Q_PROPERTY(QStringList uris READ uris WRITE setUris)
    Q_PROPERTY(uint keyStrength READ keyStrength WRITE setKeyStrength)
    Q_PROPERTY(GpgME::Subkey::PubkeyAlgo keyType READ keyType WRITE setKeyType)
    Q_PROPERTY(QString keyCurve READ keyCurve WRITE setKeyCurve)
    Q_PROPERTY(uint subkeyStrength READ subkeyStrength WRITE setSubkeyStrength)
    Q_PROPERTY(QString subkeyCurve READ subkeyCurve WRITE setSubkeyCurve)
    Q_PROPERTY(GpgME::Subkey::PubkeyAlgo subkeyType READ subkeyType WRITE setSubkeyType)
    Q_PROPERTY(bool signingAllowed READ signingAllowed WRITE setSigningAllowed)
    Q_PROPERTY(bool encryptionAllowed READ encryptionAllowed WRITE setEncryptionAllowed)
    Q_PROPERTY(bool certificationAllowed READ certificationAllowed WRITE setCertificationAllowed)
    Q_PROPERTY(bool authenticationAllowed READ authenticationAllowed WRITE setAuthenticationAllowed)
    Q_PROPERTY(QDate expiryDate READ expiryDate WRITE setExpiryDate)
public:
    explicit AdvancedSettingsDialog(QWidget *parent = nullptr);
    ~AdvancedSettingsDialog() override;

    QString dateToString(const QDate &date) const;

    QString validityPeriodHint(const QDate &minDate, const QDate &maxDate) const;

    bool unlimitedValidityIsAllowed() const;

    void setProtocol(GpgME::Protocol proto);

    void setAdditionalUserIDs(const QStringList &items);
    QStringList additionalUserIDs() const;

    void setAdditionalEMailAddresses(const QStringList &items);
    QStringList additionalEMailAddresses() const;

    void setDnsNames(const QStringList &items);
    QStringList dnsNames() const;

    void setUris(const QStringList &items);
    QStringList uris() const;

    void setKeyStrength(unsigned int strength);
    unsigned int keyStrength() const;

    void setKeyType(GpgME::Subkey::PubkeyAlgo algo);
    GpgME::Subkey::PubkeyAlgo keyType() const;

    void setKeyCurve(const QString &curve);
    QString keyCurve() const;

    void setSubkeyType(GpgME::Subkey::PubkeyAlgo algo);
    GpgME::Subkey::PubkeyAlgo subkeyType() const;

    void setSubkeyCurve(const QString &curve);
    QString subkeyCurve() const;

    void setSubkeyStrength(unsigned int strength);
    unsigned int subkeyStrength() const;

    void setSigningAllowed(bool on);
    bool signingAllowed() const;

    void setEncryptionAllowed(bool on);
    bool encryptionAllowed() const;

    void setCertificationAllowed(bool on);
    bool certificationAllowed() const;

    void setAuthenticationAllowed(bool on);
    bool authenticationAllowed() const;

    QDate forceDateIntoAllowedRange(QDate date) const;

    void setExpiryDate(QDate date);
    QDate expiryDate() const;

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void slotKeyMaterialSelectionChanged();
    void slotSigningAllowedToggled(bool on);
    void slotEncryptionAllowedToggled(bool on);

private:
    void fillKeySizeComboBoxen();
    void loadDefaultKeyType();
    void loadDefaultExpiration();
    void loadDefaultGnuPGKeyType();
    void loadDefaults();
    void updateWidgetVisibility();
    void setInitialFocus();

protected:
    void showEvent(QShowEvent *event) override;

private:
    struct UI;
    std::unique_ptr<UI> ui;

    GpgME::Protocol protocol = GpgME::UnknownProtocol;
    unsigned int pgpDefaultAlgorithm = GpgME::Subkey::AlgoELG_E;
    unsigned int cmsDefaultAlgorithm = GpgME::Subkey::AlgoRSA;
    bool keyTypeImmutable = false;
    bool mECCSupported;
    bool mEdDSASupported;
    bool isFirstShowEvent = true;
};
