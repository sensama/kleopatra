/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/newopenpgpcertificatedetailsdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "newopenpgpcertificatedetailsdialog.h"

#include "nameandemailwidget.h"

#include "newcertificatewizard/advancedsettingsdialog_p.h"
#include "newcertificatewizard/keyalgo_p.h"
#include "utils/keyparameters.h"
#include "utils/scrollarea.h"

#include <settings.h>

#include <Libkleo/Compat>
#include <Libkleo/KeyUsage>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSeparator>
#include <KSharedConfig>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <QGpgME/CryptoConfig>
#include <QGpgME/Protocol>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::NewCertificateUi;

class NewOpenPGPCertificateDetailsDialog::Private
{
    friend class ::Kleo::NewOpenPGPCertificateDetailsDialog;
    NewOpenPGPCertificateDetailsDialog *const q;

    struct UI {
        QLabel *infoLabel;
        ScrollArea *scrollArea;
        NameAndEmailWidget *nameAndEmail;
        QCheckBox *withPassCheckBox;
        QPushButton *advancedButton;
        QDialogButtonBox *buttonBox;

        UI(QWidget *dialog)
        {
            auto mainLayout = new QVBoxLayout{dialog};

            infoLabel = new QLabel{dialog};
            infoLabel->setWordWrap(true);
            mainLayout->addWidget(infoLabel);

            mainLayout->addWidget(new KSeparator{Qt::Horizontal, dialog});

            scrollArea = new ScrollArea{dialog};
            scrollArea->setFocusPolicy(Qt::NoFocus);
            scrollArea->setFrameStyle(QFrame::NoFrame);
            scrollArea->setBackgroundRole(dialog->backgroundRole());
            scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scrollArea->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
            auto scrollAreaLayout = qobject_cast<QBoxLayout *>(scrollArea->widget()->layout());
            scrollAreaLayout->setContentsMargins(0, 0, 0, 0);

            nameAndEmail = new NameAndEmailWidget{dialog};
            nameAndEmail->layout()->setContentsMargins(0, 0, 0, 0);
            scrollAreaLayout->addWidget(nameAndEmail);

            withPassCheckBox = new QCheckBox{i18n("Protect the generated key with a passphrase."), dialog};
            withPassCheckBox->setToolTip(
                i18n("Encrypts the secret key with an unrecoverable passphrase. You will be asked for the passphrase during key generation."));
            scrollAreaLayout->addWidget(withPassCheckBox);

            {
                auto layout = new QHBoxLayout;
                advancedButton = new QPushButton{i18n("Advanced Settings..."), dialog};
                advancedButton->setAutoDefault(false);
                layout->addStretch(1);
                layout->addWidget(advancedButton);
                scrollAreaLayout->addLayout(layout);
            }

            scrollAreaLayout->addStretch(1);

            mainLayout->addWidget(scrollArea);

            mainLayout->addWidget(new KSeparator{Qt::Horizontal, dialog});

            buttonBox = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog};

            mainLayout->addWidget(buttonBox);
        }
    } ui;

public:
    explicit Private(NewOpenPGPCertificateDetailsDialog *qq)
        : q{qq}
        , ui{qq}
        , advancedSettingsDlg{new AdvancedSettingsDialog{qq}}
        , technicalParameters{KeyParameters::OpenPGP}
    {
        q->setWindowTitle(i18nc("title:window", "Create OpenPGP Certificate"));

        const KConfigGroup config{KSharedConfig::openConfig(), "CertificateCreationWizard"};
        const auto attrOrder = config.readEntry("OpenPGPAttributeOrder", QStringList{});
        const auto nameIsRequired = attrOrder.contains(QLatin1String{"NAME!"}, Qt::CaseInsensitive);
        const auto emailIsRequired = attrOrder.contains(QLatin1String{"EMAIL!"}, Qt::CaseInsensitive);

        ui.infoLabel->setText(nameIsRequired || emailIsRequired //
                                  ? i18n("Enter a name and an email address to use for the certificate.")
                                  : i18n("Enter a name and/or an email address to use for the certificate."));

        ui.nameAndEmail->setNameIsRequired(nameIsRequired);
        ui.nameAndEmail->setNameLabel(config.readEntry("NAME_label"));
        ui.nameAndEmail->setNameHint(config.readEntry("NAME_hint", config.readEntry("NAME_placeholder")));
        ui.nameAndEmail->setNamePattern(config.readEntry("NAME_regex"));
        ui.nameAndEmail->setEmailIsRequired(emailIsRequired);
        ui.nameAndEmail->setEmailLabel(config.readEntry("EMAIL_label"));
        ui.nameAndEmail->setEmailHint(config.readEntry("EMAIL_hint", config.readEntry("EMAIL_placeholder")));
        ui.nameAndEmail->setEmailPattern(config.readEntry("EMAIL_regex"));

        Settings settings;
        ui.advancedButton->setVisible(!settings.hideAdvanced());

        const auto conf = QGpgME::cryptoConfig();
        const auto entry = getCryptoConfigEntry(conf, "gpg-agent", "enforce-passphrase-constraints");
        if (entry && entry->boolValue()) {
            qCDebug(KLEOPATRA_LOG) << "Disabling passphrase check box because of agent config.";
            ui.withPassCheckBox->setEnabled(false);
            ui.withPassCheckBox->setChecked(true);
        } else {
            ui.withPassCheckBox->setChecked(config.readEntry("WithPassphrase", false));
            ui.withPassCheckBox->setEnabled(!config.isEntryImmutable("WithPassphrase"));
        }

        advancedSettingsDlg->setProtocol(GpgME::OpenPGP);
        updateTechnicalParameters(); // set key parameters to default values for OpenPGP

        connect(advancedSettingsDlg, &QDialog::accepted, q, [this]() {
            updateTechnicalParameters();
        });
        connect(ui.advancedButton, &QPushButton::clicked, q, [this]() {
            advancedSettingsDlg->open();
        });
        connect(ui.buttonBox, &QDialogButtonBox::accepted, q, [this]() {
            checkAccept();
        });
        connect(ui.buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);
    }

private:
    KeyUsage keyUsage() const
    {
        KeyUsage usage;
        if (advancedSettingsDlg->signingAllowed()) {
            usage.setCanSign(true);
        }
        if (advancedSettingsDlg->encryptionAllowed() //
            && !is_ecdh(advancedSettingsDlg->subkeyType()) && !is_dsa(advancedSettingsDlg->keyType()) && !is_rsa(advancedSettingsDlg->subkeyType())) {
            usage.setCanEncrypt(true);
        }
        if (advancedSettingsDlg->authenticationAllowed()) {
            usage.setCanAuthenticate(true);
        }
        if (advancedSettingsDlg->certificationAllowed()) {
            usage.setCanCertify(true);
        }
        return usage;
    }

    KeyUsage subkeyUsage() const
    {
        KeyUsage usage;
        if (advancedSettingsDlg->encryptionAllowed()
            && (is_dsa(advancedSettingsDlg->keyType()) || is_rsa(advancedSettingsDlg->subkeyType()) || is_ecdh(advancedSettingsDlg->subkeyType()))) {
            Q_ASSERT(advancedSettingsDlg->subkeyType());
            usage.setCanEncrypt(true);
        }
        return usage;
    }

    void updateTechnicalParameters()
    {
        technicalParameters = KeyParameters{KeyParameters::OpenPGP};

        const auto keyType = advancedSettingsDlg->keyType();
        technicalParameters.setKeyType(keyType);
        if (is_ecdsa(keyType) || is_eddsa(keyType)) {
            technicalParameters.setKeyCurve(advancedSettingsDlg->keyCurve());
        } else if (const unsigned int strength = advancedSettingsDlg->keyStrength()) {
            technicalParameters.setKeyLength(strength);
        }
        technicalParameters.setKeyUsage(keyUsage());

        const auto subkeyType = advancedSettingsDlg->subkeyType();
        if (subkeyType) {
            technicalParameters.setSubkeyType(subkeyType);
            if (is_ecdh(subkeyType)) {
                technicalParameters.setSubkeyCurve(advancedSettingsDlg->subkeyCurve());
            } else if (const unsigned int strength = advancedSettingsDlg->subkeyStrength()) {
                technicalParameters.setSubkeyLength(strength);
            }
            technicalParameters.setSubkeyUsage(subkeyUsage());
        }

        if (advancedSettingsDlg->expiryDate().isValid()) {
            technicalParameters.setExpirationDate(advancedSettingsDlg->expiryDate());
        }
        // name and email are set later
    }

    void setTechnicalParameters(const KeyParameters &parameters)
    {
        advancedSettingsDlg->setKeyType(parameters.keyType());
        advancedSettingsDlg->setKeyStrength(parameters.keyLength());
        advancedSettingsDlg->setKeyCurve(parameters.keyCurve());
        advancedSettingsDlg->setSubkeyType(parameters.subkeyType());
        advancedSettingsDlg->setSubkeyStrength(parameters.subkeyLength());
        advancedSettingsDlg->setSubkeyCurve(parameters.subkeyCurve());
        advancedSettingsDlg->setSigningAllowed(parameters.keyUsage().canSign() || parameters.subkeyUsage().canSign());
        advancedSettingsDlg->setEncryptionAllowed(parameters.keyUsage().canEncrypt() || parameters.subkeyUsage().canEncrypt());
        advancedSettingsDlg->setCertificationAllowed(parameters.keyUsage().canCertify() || parameters.subkeyUsage().canCertify());
        advancedSettingsDlg->setAuthenticationAllowed(parameters.keyUsage().canAuthenticate() || parameters.subkeyUsage().canAuthenticate());
        advancedSettingsDlg->setExpiryDate(parameters.expirationDate());
    }

    void checkAccept()
    {
        QStringList errors;
        if (ui.nameAndEmail->userID().isEmpty() && !ui.nameAndEmail->nameIsRequired() && !ui.nameAndEmail->emailIsRequired()) {
            errors.push_back(i18n("Enter a name or an email address."));
        }
        const auto nameError = ui.nameAndEmail->nameError();
        if (!nameError.isEmpty()) {
            errors.push_back(nameError);
        }
        const auto emailError = ui.nameAndEmail->emailError();
        if (!emailError.isEmpty()) {
            errors.push_back(emailError);
        }
        if (errors.size() > 1) {
            KMessageBox::errorList(q, i18n("There is a problem."), errors);
        } else if (!errors.empty()) {
            KMessageBox::error(q, errors.first());
        } else {
            q->accept();
        }
    }

private:
    AdvancedSettingsDialog *const advancedSettingsDlg;
    KeyParameters technicalParameters;
};

NewOpenPGPCertificateDetailsDialog::NewOpenPGPCertificateDetailsDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog{parent, f}
    , d(new Private{this})
{
}

NewOpenPGPCertificateDetailsDialog::~NewOpenPGPCertificateDetailsDialog() = default;

void NewOpenPGPCertificateDetailsDialog::setName(const QString &name)
{
    d->ui.nameAndEmail->setName(name);
}

QString NewOpenPGPCertificateDetailsDialog::name() const
{
    return d->ui.nameAndEmail->name();
}

void NewOpenPGPCertificateDetailsDialog::setEmail(const QString &email)
{
    d->ui.nameAndEmail->setEmail(email);
}

QString NewOpenPGPCertificateDetailsDialog::email() const
{
    return d->ui.nameAndEmail->email();
}

void Kleo::NewOpenPGPCertificateDetailsDialog::setKeyParameters(const Kleo::KeyParameters &parameters)
{
    setName(parameters.name());
    const auto emails = parameters.emails();
    if (!emails.empty()) {
        setEmail(emails.front());
    }
    d->setTechnicalParameters(parameters);
}

KeyParameters NewOpenPGPCertificateDetailsDialog::keyParameters() const
{
    // set name and email on a copy of the technical parameters
    auto parameters = d->technicalParameters;
    if (!name().isEmpty()) {
        parameters.setName(name());
    }
    if (!email().isEmpty()) {
        parameters.setEmail(email());
    }
    return parameters;
}

void Kleo::NewOpenPGPCertificateDetailsDialog::setProtectKeyWithPassword(bool protectKey)
{
    d->ui.withPassCheckBox->setChecked(protectKey);
}

bool NewOpenPGPCertificateDetailsDialog::protectKeyWithPassword() const
{
    return d->ui.withPassCheckBox->isChecked();
}
