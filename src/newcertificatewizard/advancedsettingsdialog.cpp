/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/advancedsettingsdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "advancedsettingsdialog_p.h"
#include "keyalgo_p.h"
#include "listwidget.h"

#include "utils/scrollarea.h"

#include <settings.h>

#include <Libkleo/Compat>
#include <Libkleo/GnuPG>

#include <KDateComboBox>
#include <KLocalizedString>

#include <QGpgME/CryptoConfig>
#include <QGpgME/Protocol>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::NewCertificateUi;
using namespace GpgME;

static const char RSA_KEYSIZES_ENTRY[] = "RSAKeySizes";
static const char DSA_KEYSIZES_ENTRY[] = "DSAKeySizes";
static const char ELG_KEYSIZES_ENTRY[] = "ELGKeySizes";

static const char RSA_KEYSIZE_LABELS_ENTRY[] = "RSAKeySizeLabels";
static const char DSA_KEYSIZE_LABELS_ENTRY[] = "DSAKeySizeLabels";
static const char ELG_KEYSIZE_LABELS_ENTRY[] = "ELGKeySizeLabels";

static const char PGP_KEY_TYPE_ENTRY[] = "PGPKeyType";
static const char CMS_KEY_TYPE_ENTRY[] = "CMSKeyType";

// This should come from gpgme in the future
// For now we only support the basic 2.1 curves and check
// for GnuPG 2.1. The whole subkey / usage generation needs
// new api and a reworked dialog. (ah 10.3.16)
// EDDSA should be supported, too.
static const QStringList curveNames {
    { QStringLiteral("brainpoolP256r1") },
    { QStringLiteral("brainpoolP384r1") },
    { QStringLiteral("brainpoolP512r1") },
    { QStringLiteral("NIST P-256") },
    { QStringLiteral("NIST P-384") },
    { QStringLiteral("NIST P-521") },
};

namespace
{

static void set_keysize(QComboBox *cb, unsigned int strength)
{
    if (!cb) {
        return;
    }
    const int idx = cb->findData(static_cast<int>(strength));
    cb->setCurrentIndex(idx);
}

static unsigned int get_keysize(const QComboBox *cb)
{
    if (!cb) {
        return 0;
    }
    const int idx = cb->currentIndex();
    if (idx < 0) {
        return 0;
    }
    return cb->itemData(idx).toInt();
}

static void set_curve(QComboBox *cb, const QString &curve)
{
    if (!cb) {
        return;
    }
    const int idx = cb->findText(curve, Qt::MatchFixedString);
    if (idx < 0) {
        // Can't happen as we don't have them configurable.
        qCWarning(KLEOPATRA_LOG) << "curve " << curve << " not allowed";
    }
    cb->setCurrentIndex(idx);
}

static QString get_curve(const QComboBox *cb)
{
    if (!cb) {
        return QString();
    }
    return cb->currentText();
}

// Extract the algo information from default_pubkey_algo format
//
// and put it into the return values size, algo and curve.
//
// Values look like:
// RSA-2048
// rsa2048/cert,sign+rsa2048/enc
// brainpoolP256r1+brainpoolP256r1
static void parseAlgoString(const QString &algoString, int *size, Subkey::PubkeyAlgo *algo, QString &curve)
{
    const auto split = algoString.split(QLatin1Char('/'));
    bool isEncrypt = split.size() == 2 && split[1].contains(QLatin1String("enc"));

    // Normalize
    const auto lowered = split[0].toLower().remove(QLatin1Char('-'));
    if (!algo || !size) {
        return;
    }
    *algo = Subkey::AlgoUnknown;
    if (lowered.startsWith(QLatin1String("rsa"))) {
        *algo = Subkey::AlgoRSA;
    } else if (lowered.startsWith(QLatin1String("dsa"))) {
        *algo = Subkey::AlgoDSA;
    } else if (lowered.startsWith(QLatin1String("elg"))) {
        *algo = Subkey::AlgoELG;
    }

    if (*algo != Subkey::AlgoUnknown) {
        bool ok;
        *size = lowered.rightRef(lowered.size() - 3).toInt(&ok);
        if (!ok) {
            qCWarning(KLEOPATRA_LOG) << "Could not extract size from: " << lowered;
            *size = 3072;
        }
        return;
    }

    // Now the ECC Algorithms
    if (lowered.startsWith(QLatin1String("ed25519"))) {
        // Special handling for this as technically
        // this is a cv25519 curve used for EDDSA
        if (isEncrypt) {
            curve = QLatin1String("cv25519");
            *algo = Subkey::AlgoECDH;
        } else {
            curve = split[0];
            *algo = Subkey::AlgoEDDSA;
        }
        return;
    }

    if (lowered.startsWith(QLatin1String("cv25519")) ||
        lowered.startsWith(QLatin1String("nist")) ||
        lowered.startsWith(QLatin1String("brainpool")) ||
        lowered.startsWith(QLatin1String("secp"))) {
        curve = split[0];
        *algo = isEncrypt ? Subkey::AlgoECDH : Subkey::AlgoECDSA;
        return;
    }

    qCWarning(KLEOPATRA_LOG) << "Failed to parse default_pubkey_algo:" << algoString;
}

enum class OnUnlimitedValidity {
    ReturnInvalidDate,
    ReturnInternalDefault
};

QDate defaultExpirationDate(OnUnlimitedValidity onUnlimitedValidity)
{
    QDate expirationDate{};

    const auto settings = Kleo::Settings{};
    const auto defaultExpirationInDays = settings.validityPeriodInDays();
    if (defaultExpirationInDays > 0) {
        expirationDate = QDate::currentDate().addDays(defaultExpirationInDays);
    } else if (defaultExpirationInDays < 0 || onUnlimitedValidity == OnUnlimitedValidity::ReturnInternalDefault) {
        expirationDate = QDate::currentDate().addYears(2);
    }

    return expirationDate;
}

}

struct AdvancedSettingsDialog::UI
{
    QTabWidget *tabWidget = nullptr;
    QRadioButton *rsaRB = nullptr;
    QComboBox *rsaKeyStrengthCB = nullptr;
    QCheckBox *rsaSubCB = nullptr;
    QComboBox *rsaKeyStrengthSubCB = nullptr;
    QRadioButton *dsaRB = nullptr;
    QComboBox *dsaKeyStrengthCB = nullptr;
    QCheckBox *elgCB = nullptr;
    QComboBox *elgKeyStrengthCB = nullptr;
    QRadioButton *ecdsaRB = nullptr;
    QComboBox *ecdsaKeyCurvesCB = nullptr;
    QCheckBox *ecdhCB = nullptr;
    QComboBox *ecdhKeyCurvesCB = nullptr;
    QCheckBox *certificationCB = nullptr;
    QCheckBox *signingCB = nullptr;
    QCheckBox *encryptionCB = nullptr;
    QCheckBox *authenticationCB = nullptr;
    QCheckBox *expiryCB = nullptr;
    KDateComboBox *expiryDE = nullptr;
    ScrollArea *personalTab = nullptr;
    QGroupBox *uidGB = nullptr;
    Kleo::NewCertificateUi::ListWidget *uidLW = nullptr;
    QGroupBox *emailGB = nullptr;
    Kleo::NewCertificateUi::ListWidget *emailLW = nullptr;
    QGroupBox *dnsGB = nullptr;
    Kleo::NewCertificateUi::ListWidget *dnsLW = nullptr;
    QGroupBox *uriGB = nullptr;
    Kleo::NewCertificateUi::ListWidget *uriLW = nullptr;
    QDialogButtonBox *buttonBox = nullptr;

    UI(QDialog *parent)
    {
        parent->setWindowTitle(i18nc("@title:window", "Advanced Settings"));

        auto mainLayout = new QVBoxLayout{parent};

        tabWidget = new QTabWidget{parent};

        {
            auto technicalTab = new ScrollArea{tabWidget};
            technicalTab->setFocusPolicy(Qt::NoFocus);
            technicalTab->setFrameStyle(QFrame::NoFrame);
            technicalTab->setBackgroundRole(parent->backgroundRole());
            technicalTab->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            technicalTab->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
            auto tabLayout = qobject_cast<QVBoxLayout *>(technicalTab->widget()->layout());

            {
                auto groupBox = new QGroupBox{i18nc("@title:group", "Key Material"), technicalTab};
                auto groupBoxGrid = new QGridLayout{groupBox};

                int row = 0;
                rsaRB = new QRadioButton{i18nc("@option:radio", "RSA"), groupBox};
                rsaRB->setChecked(false);
                groupBoxGrid->addWidget(rsaRB, row, 0, 1, 2);

                rsaKeyStrengthCB = new QComboBox{groupBox};
                rsaKeyStrengthCB->setEnabled(false);
                groupBoxGrid->addWidget(rsaKeyStrengthCB, row, 2, 1, 1);

                row++;
                auto subKeyIndentation = new QSpacerItem(13, 13, QSizePolicy::Fixed, QSizePolicy::Minimum);
                groupBoxGrid->addItem(subKeyIndentation, row, 0, 1, 1);

                rsaSubCB = new QCheckBox{i18nc("@option:check", "+ RSA"), groupBox};
                rsaSubCB->setEnabled(true);
                groupBoxGrid->addWidget(rsaSubCB, row, 1, 1, 1);

                rsaKeyStrengthSubCB = new QComboBox{groupBox};
                rsaKeyStrengthSubCB->setEnabled(false);
                groupBoxGrid->addWidget(rsaKeyStrengthSubCB, row, 2, 1, 1);

                row++;
                dsaRB = new QRadioButton{i18nc("@option:radio", "DSA"), groupBox};
                groupBoxGrid->addWidget(dsaRB, row, 0, 1, 2);

                dsaKeyStrengthCB = new QComboBox{groupBox};
                dsaKeyStrengthCB->setEnabled(false);
                groupBoxGrid->addWidget(dsaKeyStrengthCB, row, 2, 1, 1);

                row++;
                elgCB = new QCheckBox{i18nc("@option:check", "+ Elgamal"), groupBox};
                elgCB->setToolTip(i18nc("@info:tooltip", "This subkey is required for encryption."));
                elgCB->setEnabled(true);
                groupBoxGrid->addWidget(elgCB, row, 1, 1, 1);

                elgKeyStrengthCB = new QComboBox{groupBox};
                elgKeyStrengthCB->setEnabled(false);
                groupBoxGrid->addWidget(elgKeyStrengthCB, row, 2, 1, 1);

                row++;
                ecdsaRB = new QRadioButton{i18nc("@option:radio", "ECDSA"), groupBox};
                groupBoxGrid->addWidget(ecdsaRB, row, 0, 1, 2);

                ecdsaKeyCurvesCB = new QComboBox{groupBox};
                ecdsaKeyCurvesCB->setEnabled(false);
                groupBoxGrid->addWidget(ecdsaKeyCurvesCB, row, 2, 1, 1);

                row++;
                ecdhCB = new QCheckBox{i18nc("@option:check", "+ ECDH"), groupBox};
                ecdhCB->setToolTip(i18nc("@info:tooltip", "This subkey is required for encryption."));
                ecdhCB->setEnabled(true);
                groupBoxGrid->addWidget(ecdhCB, row, 1, 1, 1);

                ecdhKeyCurvesCB = new QComboBox{groupBox};
                ecdhKeyCurvesCB->setEnabled(false);
                groupBoxGrid->addWidget(ecdhKeyCurvesCB, row, 2, 1, 1);

                groupBoxGrid->setColumnStretch(3, 1);

                tabLayout->addWidget(groupBox);
            }
            {
                auto groupBox = new QGroupBox{i18nc("@title:group", "Certificate Usage"), technicalTab};
                auto groupBoxGrid = new QGridLayout{groupBox};

                int row = 0;
                signingCB = new QCheckBox{i18nc("@option:check", "Signing"), groupBox};
                signingCB->setChecked(true);
                groupBoxGrid->addWidget(signingCB, row, 0, 1, 1);

                certificationCB = new QCheckBox{i18nc("@option:check", "Certification"), groupBox};
                groupBoxGrid->addWidget(certificationCB, row, 1, 1, 1);

                row++;
                encryptionCB = new QCheckBox{i18nc("@option:check", "Encryption"), groupBox};
                encryptionCB->setChecked(true);
                groupBoxGrid->addWidget(encryptionCB, row, 0, 1, 1);

                authenticationCB = new QCheckBox{i18nc("@option:check", "Authentication"), groupBox};
                groupBoxGrid->addWidget(authenticationCB, row, 1, 1, 1);

                row++;
                {
                    auto hbox = new QHBoxLayout;

                    expiryCB = new QCheckBox{i18nc("@option:check", "Valid until:"), groupBox};
                    hbox->addWidget(expiryCB);

                    expiryDE = new KDateComboBox(groupBox);
                    hbox->addWidget(expiryDE, 1);

                    groupBoxGrid->addLayout(hbox, row, 0, 1, 2);
                }

                tabLayout->addWidget(groupBox);
            }

            tabLayout->addStretch(1);

            tabWidget->addTab(technicalTab, i18nc("@title:tab", "Technical Details"));
        }

        {
            personalTab = new ScrollArea{tabWidget};
            personalTab->setFocusPolicy(Qt::NoFocus);
            personalTab->setFrameStyle(QFrame::NoFrame);
            personalTab->setBackgroundRole(parent->backgroundRole());
            personalTab->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            personalTab->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
            auto scrollAreaLayout = qobject_cast<QVBoxLayout *>(personalTab->widget()->layout());
            scrollAreaLayout->setContentsMargins(0, 0, 0, 0);

            auto tabGrid = new QGridLayout;

            uidGB = new QGroupBox{i18nc("@title:group", "Additional User IDs"), personalTab};
            {
                auto layout = new QVBoxLayout{uidGB};
                uidLW = new Kleo::NewCertificateUi::ListWidget{uidGB};
                layout->addWidget(uidLW);
            }
            tabGrid->addWidget(uidGB, 0, 0, 1, 2);

            emailGB = new QGroupBox{i18nc("@title:group", "EMail Addresses"), personalTab};
            {
                auto layout = new QVBoxLayout{emailGB};
                emailLW = new Kleo::NewCertificateUi::ListWidget{emailGB};
                layout->addWidget(emailLW);
            }
            tabGrid->addWidget(emailGB, 2, 0, 2, 1);

            dnsGB = new QGroupBox{i18nc("@title:group", "DNS Names"), personalTab};
            {
                auto layout = new QVBoxLayout{dnsGB};
                dnsLW = new Kleo::NewCertificateUi::ListWidget{dnsGB};
                layout->addWidget(dnsLW);
            }
            tabGrid->addWidget(dnsGB, 2, 1, 1, 1);

            uriGB = new QGroupBox{i18nc("@title:group", "URIs"), personalTab};
            {
                auto layout = new QVBoxLayout{uriGB};
                uriLW = new Kleo::NewCertificateUi::ListWidget{uriGB};
                layout->addWidget(uriLW);
            }
            tabGrid->addWidget(uriGB, 3, 1, 1, 1);

            scrollAreaLayout->addLayout(tabGrid);

            tabWidget->addTab(personalTab, i18nc("@title:tab", "Personal Details"));
        }

        mainLayout->addWidget(tabWidget);

        buttonBox = new QDialogButtonBox{parent};
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        mainLayout->addWidget(buttonBox);
    }
};

AdvancedSettingsDialog::AdvancedSettingsDialog(QWidget *parent)
    : QDialog{parent}
    , ui{new UI{this}}
    , mECCSupported{engineIsVersion(2, 1, 0)}
    , mEdDSASupported{engineIsVersion(2, 1, 15)}
{
    qRegisterMetaType<Subkey::PubkeyAlgo>("Subkey::PubkeyAlgo");

    const auto settings = Kleo::Settings{};
    {
        const auto minimumExpiry = std::max(0, settings.validityPeriodInDaysMin());
        ui->expiryDE->setMinimumDate(QDate::currentDate().addDays(minimumExpiry));
    }
    {
        const auto maximumExpiry = settings.validityPeriodInDaysMax();
        if (maximumExpiry >= 0) {
            ui->expiryDE->setMaximumDate(std::max(ui->expiryDE->minimumDate(), QDate::currentDate().addDays(maximumExpiry)));
        }
    }
    if (unlimitedValidityIsAllowed()) {
        ui->expiryDE->setEnabled(ui->expiryCB->isChecked());
    } else {
        ui->expiryCB->setEnabled(false);
        ui->expiryCB->setChecked(true);
        if (ui->expiryDE->maximumDate() == ui->expiryDE->minimumDate()) {
            // validity period is a fixed number of days
            ui->expiryDE->setEnabled(false);
        }
    }
    ui->expiryDE->setToolTip(validityPeriodHint(ui->expiryDE->minimumDate(), ui->expiryDE->maximumDate()));
    ui->emailLW->setDefaultValue(i18n("new email"));
    ui->dnsLW->setDefaultValue(i18n("new dns name"));
    ui->uriLW->setDefaultValue(i18n("new uri"));

    fillKeySizeComboBoxen();

    connect(ui->rsaRB, &QAbstractButton::toggled, ui->rsaKeyStrengthCB, &QWidget::setEnabled);
    connect(ui->rsaRB, &QAbstractButton::toggled, this, &AdvancedSettingsDialog::slotKeyMaterialSelectionChanged);
    connect(ui->rsaSubCB, &QAbstractButton::toggled, ui->rsaKeyStrengthSubCB, &QWidget::setEnabled);
    connect(ui->rsaSubCB, &QAbstractButton::toggled, this, &AdvancedSettingsDialog::slotKeyMaterialSelectionChanged);

    connect(ui->dsaRB, &QAbstractButton::toggled, ui->dsaKeyStrengthCB, &QWidget::setEnabled);
    connect(ui->dsaRB, &QAbstractButton::toggled, this, &AdvancedSettingsDialog::slotKeyMaterialSelectionChanged);
    connect(ui->elgCB, &QAbstractButton::toggled, ui->elgKeyStrengthCB, &QWidget::setEnabled);
    connect(ui->elgCB, &QAbstractButton::toggled, this, &AdvancedSettingsDialog::slotKeyMaterialSelectionChanged);

    connect(ui->ecdsaRB, &QAbstractButton::toggled, ui->ecdsaKeyCurvesCB, &QWidget::setEnabled);
    connect(ui->ecdsaRB, &QAbstractButton::toggled, this, &AdvancedSettingsDialog::slotKeyMaterialSelectionChanged);
    connect(ui->ecdhCB, &QAbstractButton::toggled, ui->ecdhKeyCurvesCB, &QWidget::setEnabled);
    connect(ui->ecdhCB, &QAbstractButton::toggled, this, &AdvancedSettingsDialog::slotKeyMaterialSelectionChanged);

    connect(ui->signingCB, &QAbstractButton::toggled, this, &AdvancedSettingsDialog::slotSigningAllowedToggled);
    connect(ui->encryptionCB, &QAbstractButton::toggled, this, &AdvancedSettingsDialog::slotEncryptionAllowedToggled);

    connect(ui->expiryCB, &QAbstractButton::toggled,
            this, [this](bool checked) {
                ui->expiryDE->setEnabled(checked);
                if (checked && !ui->expiryDE->isValid()) {
                    setExpiryDate(defaultExpirationDate(OnUnlimitedValidity::ReturnInternalDefault));
                }
            });

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

AdvancedSettingsDialog::~AdvancedSettingsDialog() = default;

QString AdvancedSettingsDialog::dateToString(const QDate &date) const
{
    // workaround for QLocale using "yy" way too often for years
    // stolen from KDateComboBox
    const auto dateFormat = (locale().dateFormat(QLocale::ShortFormat) //
                                    .replace(QLatin1String{"yy"}, QLatin1String{"yyyy"})
                                    .replace(QLatin1String{"yyyyyyyy"}, QLatin1String{"yyyy"}));
    return locale().toString(date, dateFormat);
}

QString AdvancedSettingsDialog::validityPeriodHint(const QDate &minDate, const QDate &maxDate) const
{
    // Note: minDate is always valid
    const auto today = QDate::currentDate();
    if (maxDate.isValid()) {
        if (maxDate == minDate) {
            return i18n("The validity period cannot be changed.");
        } else if (minDate == today) {
            return i18nc("... between today and <another date>.", "The validity period must end between today and %1.",
                            dateToString(maxDate));
        } else {
            return i18nc("... between <a date> and <another date>.", "The validity period must end between %1 and %2.",
                            dateToString(minDate), dateToString(maxDate));
        }
    } else {
        if (minDate == today) {
            return i18n("The validity period must end after today.");
        } else {
            return i18nc("... after <a date>.", "The validity period must end after %1.", dateToString(minDate));
        }
    }
}

bool AdvancedSettingsDialog::unlimitedValidityIsAllowed() const
{
    return !ui->expiryDE->maximumDate().isValid();
}

void AdvancedSettingsDialog::setProtocol(GpgME::Protocol proto)
{
    if (protocol == proto) {
        return;
    }
    protocol = proto;
    loadDefaults();
}

void AdvancedSettingsDialog::setAdditionalUserIDs(const QStringList &items)
{
    ui->uidLW->setItems(items);
}

QStringList AdvancedSettingsDialog::additionalUserIDs() const
{
    return ui->uidLW->items();
}

void AdvancedSettingsDialog::setAdditionalEMailAddresses(const QStringList &items)
{
    ui->emailLW->setItems(items);
}

QStringList AdvancedSettingsDialog::additionalEMailAddresses() const
{
    return ui->emailLW->items();
}

void AdvancedSettingsDialog::setDnsNames(const QStringList &items)
{
    ui->dnsLW->setItems(items);
}

QStringList AdvancedSettingsDialog::dnsNames() const
{
    return ui->dnsLW->items();
}

void AdvancedSettingsDialog::setUris(const QStringList &items)
{
    ui->uriLW->setItems(items);
}

QStringList AdvancedSettingsDialog::uris() const
{
    return ui->uriLW->items();
}

void AdvancedSettingsDialog::setKeyStrength(unsigned int strength)
{
    set_keysize(ui->rsaKeyStrengthCB, strength);
    set_keysize(ui->dsaKeyStrengthCB, strength);
}

unsigned int AdvancedSettingsDialog::keyStrength() const
{
    return
        ui->dsaRB->isChecked() ? get_keysize(ui->dsaKeyStrengthCB) :
        ui->rsaRB->isChecked() ? get_keysize(ui->rsaKeyStrengthCB) : 0;
}

void AdvancedSettingsDialog::setKeyType(Subkey::PubkeyAlgo algo)
{
    QRadioButton *const rb =
        is_rsa(algo) ? ui->rsaRB :
        is_dsa(algo) ? ui->dsaRB :
        is_ecdsa(algo) || is_eddsa(algo) ? ui->ecdsaRB : nullptr;
    if (rb) {
        rb->setChecked(true);
    }
}

Subkey::PubkeyAlgo AdvancedSettingsDialog::keyType() const
{
    return
        ui->dsaRB->isChecked() ? Subkey::AlgoDSA :
        ui->rsaRB->isChecked() ? Subkey::AlgoRSA :
        ui->ecdsaRB->isChecked() ?
            ui->ecdsaKeyCurvesCB->currentText() == QLatin1String("ed25519") ? Subkey::AlgoEDDSA :
            Subkey::AlgoECDSA :
        Subkey::AlgoUnknown;
}

void AdvancedSettingsDialog::setKeyCurve(const QString &curve)
{
    set_curve(ui->ecdsaKeyCurvesCB, curve);
}

QString AdvancedSettingsDialog::keyCurve() const
{
    return get_curve(ui->ecdsaKeyCurvesCB);
}

void AdvancedSettingsDialog::setSubkeyType(Subkey::PubkeyAlgo algo)
{
    ui->elgCB->setChecked(is_elg(algo));
    ui->rsaSubCB->setChecked(is_rsa(algo));
    ui->ecdhCB->setChecked(is_ecdh(algo));
}

Subkey::PubkeyAlgo AdvancedSettingsDialog::subkeyType() const
{
    if (ui->elgCB->isChecked()) {
        return Subkey::AlgoELG_E;
    } else if (ui->rsaSubCB->isChecked()) {
        return Subkey::AlgoRSA;
    } else if (ui->ecdhCB->isChecked()) {
        return Subkey::AlgoECDH;
    }
    return Subkey::AlgoUnknown;
}

void AdvancedSettingsDialog::setSubkeyCurve(const QString &curve)
{
    set_curve(ui->ecdhKeyCurvesCB, curve);
}

QString AdvancedSettingsDialog::subkeyCurve() const
{
    return get_curve(ui->ecdhKeyCurvesCB);
}

void AdvancedSettingsDialog::setSubkeyStrength(unsigned int strength)
{
    if (subkeyType() == Subkey::AlgoRSA) {
        set_keysize(ui->rsaKeyStrengthSubCB, strength);
    } else {
        set_keysize(ui->elgKeyStrengthCB, strength);
    }
}

unsigned int AdvancedSettingsDialog::subkeyStrength() const
{
    if (subkeyType() == Subkey::AlgoRSA) {
        return get_keysize(ui->rsaKeyStrengthSubCB);
    }
    return get_keysize(ui->elgKeyStrengthCB);
}

void AdvancedSettingsDialog::setSigningAllowed(bool on)
{
    ui->signingCB->setChecked(on);
}

bool AdvancedSettingsDialog::signingAllowed() const
{
    return ui->signingCB->isChecked();
}

void AdvancedSettingsDialog::setEncryptionAllowed(bool on)
{
    ui->encryptionCB->setChecked(on);
}

bool AdvancedSettingsDialog::encryptionAllowed() const
{
    return ui->encryptionCB->isChecked();
}

void AdvancedSettingsDialog::setCertificationAllowed(bool on)
{
    ui->certificationCB->setChecked(on);
}

bool AdvancedSettingsDialog::certificationAllowed() const
{
    return ui->certificationCB->isChecked();
}

void AdvancedSettingsDialog::setAuthenticationAllowed(bool on)
{
    ui->authenticationCB->setChecked(on);
}

bool AdvancedSettingsDialog::authenticationAllowed() const
{
    return ui->authenticationCB->isChecked();
}

QDate AdvancedSettingsDialog::forceDateIntoAllowedRange(QDate date) const
{
    const auto minDate = ui->expiryDE->minimumDate();
    if (minDate.isValid() && date < minDate) {
        date = minDate;
    }
    const auto maxDate = ui->expiryDE->maximumDate();
    if (maxDate.isValid() && date > maxDate) {
        date = maxDate;
    }
    return date;
}

void AdvancedSettingsDialog::setExpiryDate(QDate date)
{
    if (date.isValid()) {
        ui->expiryDE->setDate(forceDateIntoAllowedRange(date));
    } else {
        // check if unlimited validity is allowed
        if (unlimitedValidityIsAllowed()) {
            ui->expiryDE->setDate(date);
        }
    }
    if (ui->expiryCB->isEnabled()) {
        ui->expiryCB->setChecked(ui->expiryDE->isValid());
    }
}
QDate AdvancedSettingsDialog::expiryDate() const
{
    return ui->expiryCB->isChecked() ? forceDateIntoAllowedRange(ui->expiryDE->date()) : QDate();
}

void AdvancedSettingsDialog::slotKeyMaterialSelectionChanged()
{
    const unsigned int algo = keyType();
    const unsigned int sk_algo = subkeyType();

    if (protocol == OpenPGP) {
        // first update the enabled state, but only if key type is not forced
        if (!keyTypeImmutable) {
            ui->elgCB->setEnabled(is_dsa(algo));
            ui->rsaSubCB->setEnabled(is_rsa(algo));
            ui->ecdhCB->setEnabled(is_ecdsa(algo) || is_eddsa(algo));
            if (is_rsa(algo)) {
                ui->encryptionCB->setEnabled(true);
                ui->signingCB->setEnabled(true);
                ui->authenticationCB->setEnabled(true);
                if (is_rsa(sk_algo)) {
                    ui->encryptionCB->setEnabled(false);
                } else {
                    ui->encryptionCB->setEnabled(true);
                }
            } else if (is_dsa(algo)) {
                ui->encryptionCB->setEnabled(false);
            } else if (is_ecdsa(algo) || is_eddsa(algo)) {
                ui->signingCB->setEnabled(true);
                ui->authenticationCB->setEnabled(true);
                ui->encryptionCB->setEnabled(false);
            }
        }
        // then update the checked state
        if (sender() == ui->dsaRB || sender() == ui->rsaRB || sender() == ui->ecdsaRB) {
            ui->elgCB->setChecked(is_dsa(algo));
            ui->ecdhCB->setChecked(is_ecdsa(algo) || is_eddsa(algo));
            ui->rsaSubCB->setChecked(is_rsa(algo));
        }
        if (is_rsa(algo)) {
            ui->encryptionCB->setChecked(true);
            ui->signingCB->setChecked(true);
            if (is_rsa(sk_algo)) {
                ui->encryptionCB->setChecked(true);
            }
        } else if (is_dsa(algo)) {
            if (is_elg(sk_algo)) {
                ui->encryptionCB->setChecked(true);
            } else {
                ui->encryptionCB->setChecked(false);
            }
        } else if (is_ecdsa(algo) || is_eddsa(algo)) {
            ui->signingCB->setChecked(true);
            ui->encryptionCB->setChecked(is_ecdh(sk_algo));
        }
    } else {
        //assert( is_rsa( keyType() ) ); // it can happen through misconfiguration by the admin that no key type is selectable at all
    }
}

void AdvancedSettingsDialog::slotSigningAllowedToggled(bool on)
{
    if (!on && protocol == CMS && !encryptionAllowed()) {
        setEncryptionAllowed(true);
    }
}

void AdvancedSettingsDialog::slotEncryptionAllowedToggled(bool on)
{
    if (!on && protocol == CMS && !signingAllowed()) {
        setSigningAllowed(true);
    }
}

static void fill_combobox(QComboBox &cb, const QList<int> &sizes, const QStringList &labels)
{
    cb.clear();
    for (int i = 0, end = sizes.size(); i != end; ++i) {
        const int size = std::abs(sizes[i]);
        /* As we respect the defaults configurable in GnuPG, and we also have configurable
         * defaults in Kleopatra its difficult to print out "default" here. To avoid confusion
         * about that its better not to show any default indication.  */
        cb.addItem(i < labels.size() && !labels[i].trimmed().isEmpty()
                   ? i18ncp("%2: some admin-supplied text, %1: key size in bits", "%2 (1 bit)", "%2 (%1 bits)", size, labels[i].trimmed())
                   : i18ncp("%1: key size in bits", "1 bit", "%1 bits", size),
                   size);
        if (sizes[i] < 0) {
            cb.setCurrentIndex(cb.count() - 1);
        }
    }
}

void AdvancedSettingsDialog::fillKeySizeComboBoxen()
{

    const KConfigGroup config(KSharedConfig::openConfig(), "CertificateCreationWizard");

    QList<int> rsaKeySizes = config.readEntry(RSA_KEYSIZES_ENTRY, QList<int>() << 2048 << -3072 << 4096);
    if (Kleo::gnupgUsesDeVsCompliance()) {
        rsaKeySizes = config.readEntry(RSA_KEYSIZES_ENTRY, QList<int>() << -3072 << 4096);
    }
    const QList<int> dsaKeySizes = config.readEntry(DSA_KEYSIZES_ENTRY, QList<int>() << -2048);
    const QList<int> elgKeySizes = config.readEntry(ELG_KEYSIZES_ENTRY, QList<int>() << -2048 << 3072 << 4096);

    const QStringList rsaKeySizeLabels = config.readEntry(RSA_KEYSIZE_LABELS_ENTRY, QStringList());
    const QStringList dsaKeySizeLabels = config.readEntry(DSA_KEYSIZE_LABELS_ENTRY, QStringList());
    const QStringList elgKeySizeLabels = config.readEntry(ELG_KEYSIZE_LABELS_ENTRY, QStringList());

    fill_combobox(*ui->rsaKeyStrengthCB, rsaKeySizes, rsaKeySizeLabels);
    fill_combobox(*ui->rsaKeyStrengthSubCB, rsaKeySizes, rsaKeySizeLabels);
    fill_combobox(*ui->dsaKeyStrengthCB, dsaKeySizes, dsaKeySizeLabels);
    fill_combobox(*ui->elgKeyStrengthCB, elgKeySizes, elgKeySizeLabels);
    if (mEdDSASupported) {
        // If supported we recommend cv25519
        ui->ecdsaKeyCurvesCB->addItem(QStringLiteral("ed25519"));
        ui->ecdhKeyCurvesCB->addItem(QStringLiteral("cv25519"));
    }
    ui->ecdhKeyCurvesCB->addItems(curveNames);
    ui->ecdsaKeyCurvesCB->addItems(curveNames);
}

// Try to load the default key type from GnuPG
void AdvancedSettingsDialog::loadDefaultGnuPGKeyType()
{
    const auto conf = QGpgME::cryptoConfig();
    if (!conf) {
        qCWarning(KLEOPATRA_LOG) << "Failed to obtain cryptoConfig.";
        return;
    }
    const auto entry = getCryptoConfigEntry(conf, protocol == CMS ? "gpgsm" : "gpg", "default_pubkey_algo");
    if (!entry) {
        qCDebug(KLEOPATRA_LOG) << "GnuPG does not have default key type. Fallback to RSA";
        setKeyType(Subkey::AlgoRSA);
        setSubkeyType(Subkey::AlgoRSA);
        return;
    }

    qCDebug(KLEOPATRA_LOG) << "Have default key type: " << entry->stringValue();

    // Format is <primarytype>[/usage]+<subkeytype>[/usage]
    const auto split = entry->stringValue().split(QLatin1Char('+'));
    int size = 0;
    Subkey::PubkeyAlgo algo = Subkey::AlgoUnknown;
    QString curve;

    parseAlgoString(split[0], &size, &algo, curve);
    if (algo == Subkey::AlgoUnknown) {
        setSubkeyType(Subkey::AlgoRSA);
        return;
    }

    setKeyType(algo);

    if (is_rsa(algo) || is_elg(algo) || is_dsa(algo)) {
        setKeyStrength(size);
    } else {
        setKeyCurve(curve);
    }

    {
        auto algoString = (split.size() == 2) ? split[1] : split[0];
        // If it has no usage we assume encrypt subkey
        if (!algoString.contains(QLatin1Char('/'))) {
            algoString += QStringLiteral("/enc");
        }

        parseAlgoString(algoString, &size, &algo, curve);

        if (algo == Subkey::AlgoUnknown) {
            setSubkeyType(Subkey::AlgoRSA);
            return;
        }

        setSubkeyType(algo);

        if (is_rsa(algo) || is_elg(algo)) {
            setSubkeyStrength(size);
        } else {
            setSubkeyCurve(curve);
        }
    }
}

void AdvancedSettingsDialog::loadDefaultKeyType()
{

    if (protocol != CMS && protocol != OpenPGP) {
        return;
    }

    const KConfigGroup config(KSharedConfig::openConfig(), "CertificateCreationWizard");

    const QString entry = protocol == CMS ? QLatin1String(CMS_KEY_TYPE_ENTRY) : QLatin1String(PGP_KEY_TYPE_ENTRY);
    const QString keyType = config.readEntry(entry).trimmed().toUpper();

    if (protocol == OpenPGP && keyType == QLatin1String("DSA")) {
        setKeyType(Subkey::AlgoDSA);
        setSubkeyType(Subkey::AlgoUnknown);
    } else if (protocol == OpenPGP && keyType == QLatin1String("DSA+ELG")) {
        setKeyType(Subkey::AlgoDSA);
        setSubkeyType(Subkey::AlgoELG_E);
    } else if (keyType.isEmpty() && engineIsVersion(2, 1, 17)) {
        loadDefaultGnuPGKeyType();
    } else {
        if (!keyType.isEmpty() && keyType != QLatin1String("RSA"))
            qCWarning(KLEOPATRA_LOG) << "invalid value \"" << qPrintable(keyType)
                                     << "\" for entry \"[CertificateCreationWizard]"
                                     << qPrintable(entry) << "\"";
        setKeyType(Subkey::AlgoRSA);
        setSubkeyType(Subkey::AlgoRSA);
    }

    keyTypeImmutable = config.isEntryImmutable(entry);
}

void AdvancedSettingsDialog::loadDefaultExpiration()
{
    if (protocol != OpenPGP) {
        return;
    }

    if (unlimitedValidityIsAllowed()) {
        setExpiryDate(defaultExpirationDate(OnUnlimitedValidity::ReturnInvalidDate));
    } else {
        setExpiryDate(defaultExpirationDate(OnUnlimitedValidity::ReturnInternalDefault));
    }
}

void AdvancedSettingsDialog::loadDefaults()
{
    loadDefaultKeyType();
    loadDefaultExpiration();

    updateWidgetVisibility();
    setInitialFocus();
}

void AdvancedSettingsDialog::updateWidgetVisibility()
{
    // Personal Details Page
    if (protocol == OpenPGP) {    // ### hide until multi-uid is implemented
        if (ui->tabWidget->indexOf(ui->personalTab) != -1) {
            ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->personalTab));
        }
    } else {
        if (ui->tabWidget->indexOf(ui->personalTab) == -1) {
            ui->tabWidget->addTab(ui->personalTab, i18nc("@title:tab", "Personal Details"));
        }
    }
    ui->uidGB->setVisible(protocol == OpenPGP);
    ui->uidGB->setEnabled(false);
    ui->uidGB->setToolTip(i18nc("@info:tooltip", "Adding more than one user ID is not yet implemented."));
    ui->emailGB->setVisible(protocol == CMS);
    ui->dnsGB->setVisible(protocol == CMS);
    ui->uriGB->setVisible(protocol == CMS);

    // Technical Details Page
    ui->ecdhCB->setVisible(mECCSupported);
    ui->ecdhKeyCurvesCB->setVisible(mECCSupported);
    ui->ecdsaKeyCurvesCB->setVisible(mECCSupported);
    ui->ecdsaRB->setVisible(mECCSupported);
    if (mEdDSASupported) {
        // We use the same radio button for EdDSA as we use for
        // ECDSA GnuPG does the same and this is really super technical
        // land.
        ui->ecdsaRB->setText(QStringLiteral("ECDSA/EdDSA"));
    }

    const bool deVsHack = Kleo::gnupgUsesDeVsCompliance();

    if (deVsHack) {
        // GnuPG Provides no API to query which keys are compliant for
        // a mode. If we request a different one it will error out so
        // we have to remove the options.
        //
        // Does anyone want to use NIST anyway?
        int i;
        while ((i = ui->ecdsaKeyCurvesCB->findText(QStringLiteral("NIST"), Qt::MatchStartsWith)) != -1 ||
               (i = ui->ecdsaKeyCurvesCB->findText(QStringLiteral("25519"), Qt::MatchEndsWith)) != -1) {
            ui->ecdsaKeyCurvesCB->removeItem(i);
        }
        while ((i = ui->ecdhKeyCurvesCB->findText(QStringLiteral("NIST"), Qt::MatchStartsWith)) != -1 ||
               (i = ui->ecdhKeyCurvesCB->findText(QStringLiteral("25519"), Qt::MatchEndsWith)) != -1) {
            ui->ecdhKeyCurvesCB->removeItem(i);
        }
    }
    ui->certificationCB->setVisible(protocol == OpenPGP);   // gpgsm limitation?
    ui->authenticationCB->setVisible(protocol == OpenPGP);

    if (keyTypeImmutable) {
        ui->rsaRB->setEnabled(false);
        ui->rsaSubCB->setEnabled(false);
        ui->dsaRB->setEnabled(false);
        ui->elgCB->setEnabled(false);
        ui->ecdsaRB->setEnabled(false);
        ui->ecdhCB->setEnabled(false);

        // force usage if key type is forced
        ui->certificationCB->setEnabled(false);
        ui->signingCB->setEnabled(false);
        ui->encryptionCB->setEnabled(false);
        ui->authenticationCB->setEnabled(false);
    } else {
        ui->rsaRB->setEnabled(true);
        ui->rsaSubCB->setEnabled(protocol == OpenPGP);
        ui->dsaRB->setEnabled(protocol == OpenPGP && !deVsHack);
        ui->elgCB->setEnabled(protocol == OpenPGP && !deVsHack);
        ui->ecdsaRB->setEnabled(protocol == OpenPGP);
        ui->ecdhCB->setEnabled(protocol == OpenPGP);

        if (protocol == OpenPGP) {
            // OpenPGP keys must have certify capability
            ui->certificationCB->setEnabled(false);
        }
        if (protocol == CMS) {
            ui->encryptionCB->setEnabled(true);
            ui->rsaKeyStrengthSubCB->setEnabled(false);
        }
    }
    if (protocol == OpenPGP) {
        // OpenPGP keys must have certify capability
        ui->certificationCB->setChecked(true);
    }
    if (protocol == CMS) {
        ui->rsaSubCB->setChecked(false);
    }

    ui->expiryDE->setVisible(protocol == OpenPGP);
    ui->expiryCB->setVisible(protocol == OpenPGP);

    slotKeyMaterialSelectionChanged();
}

template<typename UnaryPredicate>
bool focusFirstButtonIf(const std::vector<QAbstractButton *> &buttons, UnaryPredicate p)
{
    auto it = std::find_if(std::begin(buttons), std::end(buttons), p);
    if (it != std::end(buttons)) {
        (*it)->setFocus();
        return true;
    }
    return false;
}

static bool focusFirstCheckedButton(const std::vector<QAbstractButton *> &buttons)
{
    return focusFirstButtonIf(buttons,
                              [](auto btn) {
                                  return btn && btn->isEnabled() && btn->isChecked();
                              });
}

static bool focusFirstEnabledButton(const std::vector<QAbstractButton *> &buttons)
{
    return focusFirstButtonIf(buttons,
                              [](auto btn) {
                                  return btn && btn->isEnabled();
                              });
}

void AdvancedSettingsDialog::setInitialFocus()
{
    // first try the key type radio buttons
    if (focusFirstCheckedButton({ui->rsaRB, ui->dsaRB, ui->ecdsaRB})) {
        return;
    }
    // then try the usage check boxes and the expiration check box
    if (focusFirstEnabledButton({ui->signingCB, ui->certificationCB, ui->encryptionCB, ui->authenticationCB, ui->expiryCB})) {
        return;
    }
    // finally, focus the OK button
    ui->buttonBox->button(QDialogButtonBox::Ok)->setFocus();
}
