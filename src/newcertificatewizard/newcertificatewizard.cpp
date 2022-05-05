/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/newcertificatewizard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "newcertificatewizard.h"

#include <settings.h>

#include "ui_chooseprotocolpage.h"
#include "ui_enterdetailspage.h"
#include "ui_keycreationpage.h"
#include "ui_resultpage.h"

#include "ui_advancedsettingsdialog.h"

#ifdef QGPGME_SUPPORTS_SECRET_KEY_EXPORT
# include "commands/exportsecretkeycommand.h"
#else
# include "commands/exportsecretkeycommand_old.h"
#endif
#include "commands/exportopenpgpcertstoservercommand.h"
#include "commands/exportcertificatecommand.h"

#include "kleopatraapplication.h"

#include "utils/validation.h"
#include "utils/filedialog.h"
#include "utils/keyparameters.h"
#include "utils/userinfo.h"

#include <Libkleo/Compat>
#include <Libkleo/GnuPG>
#include <Libkleo/Stl_Util>
#include <Libkleo/Dn>
#include <Libkleo/OidMap>
#include <Libkleo/KeyCache>
#include <Libkleo/Formatting>

#include <QGpgME/KeyGenerationJob>
#include <QGpgME/Protocol>
#include <QGpgME/CryptoConfig>

#include <gpgme++/global.h>
#include <gpgme++/keygenerationresult.h>
#include <gpgme++/context.h>
#include <gpgme++/interfaces/passphraseprovider.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include "kleopatra_debug.h"
#include <QTemporaryDir>
#include <KMessageBox>
#include <QIcon>

#include <QRegularExpressionValidator>
#include <QLineEdit>
#include <QMetaProperty>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QDesktopServices>
#include <QUrlQuery>

#include <algorithm>

#include <KSharedConfig>
#include <QLocale>

using namespace Kleo;
using namespace Kleo::NewCertificateUi;
using namespace Kleo::Commands;
using namespace GpgME;
#ifndef QGPGME_SUPPORTS_SECRET_KEY_EXPORT
using Kleo::Commands::Compat::ExportSecretKeyCommand;
#endif

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

class EmptyPassphraseProvider: public PassphraseProvider
{
public:
    char *getPassphrase(const char * /*useridHint*/, const char * /*description*/,
                        bool /*previousWasBad*/, bool &/*canceled*/) Q_DECL_OVERRIDE
    {
        return gpgrt_strdup ("");
    }
};

static void set_tab_order(const QList<QWidget *> &wl)
{
    kdtools::for_each_adjacent_pair(wl.begin(), wl.end(), &QWidget::setTabOrder);
}

enum KeyAlgo { RSA, DSA, ELG, ECDSA, ECDH, EDDSA };

static bool is_algo(Subkey::PubkeyAlgo algo, KeyAlgo what)
{
    switch (algo) {
        case Subkey::AlgoRSA:
        case Subkey::AlgoRSA_E:
        case Subkey::AlgoRSA_S:
            return what == RSA;
        case Subkey::AlgoELG_E:
        case Subkey::AlgoELG:
            return what == ELG;
        case Subkey::AlgoDSA:
            return what == DSA;
        case Subkey::AlgoECDSA:
            return what == ECDSA;
        case Subkey::AlgoECDH:
            return what == ECDH;
        case Subkey::AlgoEDDSA:
            return what == EDDSA;
        default:
            break;
    }
    return false;
}

static bool is_rsa(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), RSA);
}

static bool is_dsa(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), DSA);
}

static bool is_elg(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), ELG);
}

static bool is_ecdsa(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), ECDSA);
}

static bool is_eddsa(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), EDDSA);
}

static bool is_ecdh(unsigned int algo)
{
    return is_algo(static_cast<Subkey::PubkeyAlgo>(algo), ECDH);
}

static void force_set_checked(QAbstractButton *b, bool on)
{
    // work around Qt bug (tested: 4.1.4, 4.2.3, 4.3.4)
    const bool autoExclusive = b->autoExclusive();
    b->setAutoExclusive(false);
    b->setChecked(b->isEnabled() && on);
    b->setAutoExclusive(autoExclusive);
}

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

Q_DECLARE_METATYPE(GpgME::Subkey::PubkeyAlgo)
namespace Kleo
{
namespace NewCertificateUi
{
class WizardPage : public QWizardPage
{
    Q_OBJECT
protected:
    explicit WizardPage(QWidget *parent = nullptr)
        : QWizardPage(parent) {}

    NewCertificateWizard *wizard() const
    {
        Q_ASSERT(static_cast<NewCertificateWizard *>(QWizardPage::wizard()) == qobject_cast<NewCertificateWizard *>(QWizardPage::wizard()));
        return static_cast<NewCertificateWizard *>(QWizardPage::wizard());
    }

    void resetProtocol()
    {
        wizard()->resetProtocol();
    }

    void restartAtEnterDetailsPage()
    {
        wizard()->restartAtEnterDetailsPage();
    }

    QDir tmpDir() const;

protected:
#define FIELD(type, name) type name() const { return field( QStringLiteral(#name) ).value<type>(); }
    FIELD(bool, pgp)
    FIELD(bool, signingAllowed)
    FIELD(bool, encryptionAllowed)
    FIELD(bool, certificationAllowed)
    FIELD(bool, authenticationAllowed)

    FIELD(QString, name)
    FIELD(QString, email)
    FIELD(QString, dn)
    FIELD(bool, protectedKey)

    FIELD(Subkey::PubkeyAlgo, keyType)
    FIELD(int, keyStrength)
    FIELD(QString, keyCurve)

    FIELD(Subkey::PubkeyAlgo, subkeyType)
    FIELD(int, subkeyStrength)
    FIELD(QString, subkeyCurve)

    FIELD(QDate, expiryDate)

    FIELD(QStringList, additionalUserIDs)
    FIELD(QStringList, additionalEMailAddresses)
    FIELD(QStringList, dnsNames)
    FIELD(QStringList, uris)

    FIELD(QString, url)
    FIELD(QString, error)
    FIELD(QString, result)
    FIELD(QString, fingerprint)
#undef FIELD
};
} // namespace NewCertificateUi
} // namespace Kleo

using namespace Kleo::NewCertificateUi;

namespace
{

class AdvancedSettingsDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QStringList additionalUserIDs READ additionalUserIDs WRITE setAdditionalUserIDs)
    Q_PROPERTY(QStringList additionalEMailAddresses READ additionalEMailAddresses WRITE setAdditionalEMailAddresses)
    Q_PROPERTY(QStringList dnsNames READ dnsNames WRITE setDnsNames)
    Q_PROPERTY(QStringList uris READ uris WRITE setUris)
    Q_PROPERTY(uint keyStrength READ keyStrength WRITE setKeyStrength)
    Q_PROPERTY(Subkey::PubkeyAlgo keyType READ keyType WRITE setKeyType)
    Q_PROPERTY(QString keyCurve READ keyCurve WRITE setKeyCurve)
    Q_PROPERTY(uint subkeyStrength READ subkeyStrength WRITE setSubkeyStrength)
    Q_PROPERTY(QString subkeyCurve READ subkeyCurve WRITE setSubkeyCurve)
    Q_PROPERTY(Subkey::PubkeyAlgo subkeyType READ subkeyType WRITE setSubkeyType)
    Q_PROPERTY(bool signingAllowed READ signingAllowed WRITE setSigningAllowed)
    Q_PROPERTY(bool encryptionAllowed READ encryptionAllowed WRITE setEncryptionAllowed)
    Q_PROPERTY(bool certificationAllowed READ certificationAllowed WRITE setCertificationAllowed)
    Q_PROPERTY(bool authenticationAllowed READ authenticationAllowed WRITE setAuthenticationAllowed)
    Q_PROPERTY(QDate expiryDate READ expiryDate WRITE setExpiryDate)
public:
    explicit AdvancedSettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent),
          protocol(UnknownProtocol),
          pgpDefaultAlgorithm(Subkey::AlgoELG_E),
          cmsDefaultAlgorithm(Subkey::AlgoRSA),
          keyTypeImmutable(false),
          ui(),
          mECCSupported(engineIsVersion(2, 1, 0)),
          mEdDSASupported(engineIsVersion(2, 1, 15))
    {
        qRegisterMetaType<Subkey::PubkeyAlgo>("Subkey::PubkeyAlgo");
        ui.setupUi(this);

        const auto settings = Kleo::Settings{};
        {
            const auto minimumExpiry = std::max(0, settings.validityPeriodInDaysMin());
            ui.expiryDE->setMinimumDate(QDate::currentDate().addDays(minimumExpiry));
        }
        {
            const auto maximumExpiry = settings.validityPeriodInDaysMax();
            if (maximumExpiry >= 0) {
                ui.expiryDE->setMaximumDate(std::max(ui.expiryDE->minimumDate(), QDate::currentDate().addDays(maximumExpiry)));
            }
        }
        if (unlimitedValidityIsAllowed()) {
            ui.expiryDE->setEnabled(ui.expiryCB->isChecked());
        } else {
            ui.expiryCB->setEnabled(false);
            ui.expiryCB->setChecked(true);
            if (ui.expiryDE->maximumDate() == ui.expiryDE->minimumDate()) {
                // validity period is a fixed number of days
                ui.expiryDE->setEnabled(false);
            }
        }
        ui.expiryDE->setToolTip(validityPeriodHint(ui.expiryDE->minimumDate(), ui.expiryDE->maximumDate()));
        ui.emailLW->setDefaultValue(i18n("new email"));
        ui.dnsLW->setDefaultValue(i18n("new dns name"));
        ui.uriLW->setDefaultValue(i18n("new uri"));
        ui.elgCB->setToolTip(i18nc("@info:tooltip", "This subkey is required for encryption."));
        ui.ecdhCB->setToolTip(i18nc("@info:tooltip", "This subkey is required for encryption."));

        fillKeySizeComboBoxen();

        connect(ui.expiryCB, &QAbstractButton::toggled,
                this, [this](bool checked) {
                    ui.expiryDE->setEnabled(checked);
                    if (checked && !ui.expiryDE->isValid()) {
                        setExpiryDate(defaultExpirationDate(OnUnlimitedValidity::ReturnInternalDefault));
                    }
                });
    }

    QString dateToString(const QDate &date) const
    {
        // workaround for QLocale using "yy" way too often for years
        // stolen from KDateComboBox
        const auto dateFormat = (locale().dateFormat(QLocale::ShortFormat) //
                                     .replace(QLatin1String{"yy"}, QLatin1String{"yyyy"})
                                     .replace(QLatin1String{"yyyyyyyy"}, QLatin1String{"yyyy"}));
        return locale().toString(date, dateFormat);
    }

    QString validityPeriodHint(const QDate &minDate, const QDate &maxDate) const
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

    bool unlimitedValidityIsAllowed() const
    {
        return !ui.expiryDE->maximumDate().isValid();
    }

    void setProtocol(GpgME::Protocol proto)
    {
        if (protocol == proto) {
            return;
        }
        protocol = proto;
        loadDefaults();
    }

    void setAdditionalUserIDs(const QStringList &items)
    {
        ui.uidLW->setItems(items);
    }
    QStringList additionalUserIDs() const
    {
        return ui.uidLW->items();
    }

    void setAdditionalEMailAddresses(const QStringList &items)
    {
        ui.emailLW->setItems(items);
    }
    QStringList additionalEMailAddresses() const
    {
        return ui.emailLW->items();
    }

    void setDnsNames(const QStringList &items)
    {
        ui.dnsLW->setItems(items);
    }
    QStringList dnsNames() const
    {
        return ui.dnsLW->items();
    }

    void setUris(const QStringList &items)
    {
        ui.uriLW->setItems(items);
    }
    QStringList uris() const
    {
        return ui.uriLW->items();
    }

    void setKeyStrength(unsigned int strength)
    {
        set_keysize(ui.rsaKeyStrengthCB, strength);
        set_keysize(ui.dsaKeyStrengthCB, strength);
    }
    unsigned int keyStrength() const
    {
        return
            ui.dsaRB->isChecked() ? get_keysize(ui.dsaKeyStrengthCB) :
            ui.rsaRB->isChecked() ? get_keysize(ui.rsaKeyStrengthCB) : 0;
    }

    void setKeyType(Subkey::PubkeyAlgo algo)
    {
        QRadioButton *const rb =
            is_rsa(algo) ? ui.rsaRB :
            is_dsa(algo) ? ui.dsaRB :
            is_ecdsa(algo) || is_eddsa(algo) ? ui.ecdsaRB : nullptr;
        if (rb) {
            rb->setChecked(true);
        }
    }
    Subkey::PubkeyAlgo keyType() const
    {
        return
            ui.dsaRB->isChecked() ? Subkey::AlgoDSA :
            ui.rsaRB->isChecked() ? Subkey::AlgoRSA :
            ui.ecdsaRB->isChecked() ?
                ui.ecdsaKeyCurvesCB->currentText() == QLatin1String("ed25519") ? Subkey::AlgoEDDSA :
                Subkey::AlgoECDSA :
            Subkey::AlgoUnknown;
    }

    void setKeyCurve(const QString &curve)
    {
        set_curve(ui.ecdsaKeyCurvesCB, curve);
    }

    QString keyCurve() const
    {
        return get_curve(ui.ecdsaKeyCurvesCB);
    }

    void setSubkeyType(Subkey::PubkeyAlgo algo)
    {
        ui.elgCB->setChecked(is_elg(algo));
        ui.rsaSubCB->setChecked(is_rsa(algo));
        ui.ecdhCB->setChecked(is_ecdh(algo));
    }
    Subkey::PubkeyAlgo subkeyType() const
    {
        if (ui.elgCB->isChecked()) {
            return Subkey::AlgoELG_E;
        } else if (ui.rsaSubCB->isChecked()) {
            return Subkey::AlgoRSA;
        } else if (ui.ecdhCB->isChecked()) {
            return Subkey::AlgoECDH;
        }
        return Subkey::AlgoUnknown;
    }

    void setSubkeyCurve(const QString &curve)
    {
        set_curve(ui.ecdhKeyCurvesCB, curve);
    }

    QString subkeyCurve() const
    {
        return get_curve(ui.ecdhKeyCurvesCB);
    }

    void setSubkeyStrength(unsigned int strength)
    {
        if (subkeyType() == Subkey::AlgoRSA) {
            set_keysize(ui.rsaKeyStrengthSubCB, strength);
        } else {
            set_keysize(ui.elgKeyStrengthCB, strength);
        }
    }
    unsigned int subkeyStrength() const
    {
        if (subkeyType() == Subkey::AlgoRSA) {
            return get_keysize(ui.rsaKeyStrengthSubCB);
        }
        return get_keysize(ui.elgKeyStrengthCB);
    }

    void setSigningAllowed(bool on)
    {
        ui.signingCB->setChecked(on);
    }
    bool signingAllowed() const
    {
        return ui.signingCB->isChecked();
    }

    void setEncryptionAllowed(bool on)
    {
        ui.encryptionCB->setChecked(on);
    }
    bool encryptionAllowed() const
    {
        return ui.encryptionCB->isChecked();
    }

    void setCertificationAllowed(bool on)
    {
        ui.certificationCB->setChecked(on);
    }
    bool certificationAllowed() const
    {
        return ui.certificationCB->isChecked();
    }

    void setAuthenticationAllowed(bool on)
    {
        ui.authenticationCB->setChecked(on);
    }
    bool authenticationAllowed() const
    {
        return ui.authenticationCB->isChecked();
    }

    QDate forceDateIntoAllowedRange(QDate date) const
    {
        const auto minDate = ui.expiryDE->minimumDate();
        if (minDate.isValid() && date < minDate) {
            date = minDate;
        }
        const auto maxDate = ui.expiryDE->maximumDate();
        if (maxDate.isValid() && date > maxDate) {
            date = maxDate;
        }
        return date;
    }

    void setExpiryDate(QDate date)
    {
        if (date.isValid()) {
            ui.expiryDE->setDate(forceDateIntoAllowedRange(date));
        } else {
            // check if unlimited validity is allowed
            if (unlimitedValidityIsAllowed()) {
                ui.expiryDE->setDate(date);
            }
        }
        if (ui.expiryCB->isEnabled()) {
            ui.expiryCB->setChecked(ui.expiryDE->isValid());
        }
    }
    QDate expiryDate() const
    {
        return ui.expiryCB->isChecked() ? forceDateIntoAllowedRange(ui.expiryDE->date()) : QDate();
    }

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void slotKeyMaterialSelectionChanged()
    {
        const unsigned int algo = keyType();
        const unsigned int sk_algo = subkeyType();

        if (protocol == OpenPGP) {
            // first update the enabled state, but only if key type is not forced
            if (!keyTypeImmutable) {
                ui.elgCB->setEnabled(is_dsa(algo));
                ui.rsaSubCB->setEnabled(is_rsa(algo));
                ui.ecdhCB->setEnabled(is_ecdsa(algo) || is_eddsa(algo));
                if (is_rsa(algo)) {
                    ui.encryptionCB->setEnabled(true);
                    ui.signingCB->setEnabled(true);
                    ui.authenticationCB->setEnabled(true);
                    if (is_rsa(sk_algo)) {
                        ui.encryptionCB->setEnabled(false);
                    } else {
                        ui.encryptionCB->setEnabled(true);
                    }
                } else if (is_dsa(algo)) {
                    ui.encryptionCB->setEnabled(false);
                } else if (is_ecdsa(algo) || is_eddsa(algo)) {
                    ui.signingCB->setEnabled(true);
                    ui.authenticationCB->setEnabled(true);
                    ui.encryptionCB->setEnabled(false);
                }
            }
            // then update the checked state
            if (sender() == ui.dsaRB || sender() == ui.rsaRB || sender() == ui.ecdsaRB) {
                ui.elgCB->setChecked(is_dsa(algo));
                ui.ecdhCB->setChecked(is_ecdsa(algo) || is_eddsa(algo));
                ui.rsaSubCB->setChecked(is_rsa(algo));
            }
            if (is_rsa(algo)) {
                ui.encryptionCB->setChecked(true);
                ui.signingCB->setChecked(true);
                if (is_rsa(sk_algo)) {
                    ui.encryptionCB->setChecked(true);
                }
            } else if (is_dsa(algo)) {
                if (is_elg(sk_algo)) {
                    ui.encryptionCB->setChecked(true);
                } else {
                    ui.encryptionCB->setChecked(false);
                }
            } else if (is_ecdsa(algo) || is_eddsa(algo)) {
                ui.signingCB->setChecked(true);
                ui.encryptionCB->setChecked(is_ecdh(sk_algo));
            }
        } else {
            //assert( is_rsa( keyType() ) ); // it can happen through misconfiguration by the admin that no key type is selectable at all
        }
    }

    void slotSigningAllowedToggled(bool on)
    {
        if (!on && protocol == CMS && !encryptionAllowed()) {
            setEncryptionAllowed(true);
        }
    }
    void slotEncryptionAllowedToggled(bool on)
    {
        if (!on && protocol == CMS && !signingAllowed()) {
            setSigningAllowed(true);
        }
    }

private:
    void fillKeySizeComboBoxen();
    void loadDefaultKeyType();
    void loadDefaultExpiration();
    void loadDefaultGnuPGKeyType();
    void loadDefaults();
    void updateWidgetVisibility();
    void setInitialFocus();

private:
    GpgME::Protocol protocol;
    unsigned int pgpDefaultAlgorithm;
    unsigned int cmsDefaultAlgorithm;
    bool keyTypeImmutable;
    Ui_AdvancedSettingsDialog ui;
    bool mECCSupported;
    bool mEdDSASupported;
};

class ChooseProtocolPage : public WizardPage
{
    Q_OBJECT
public:
    explicit ChooseProtocolPage(QWidget *p = nullptr)
        : WizardPage(p),
          initialized(false),
          ui()
    {
        ui.setupUi(this);
        ui.pgpCLB->setAccessibleDescription(ui.pgpCLB->description());
        ui.x509CLB->setAccessibleDescription(ui.x509CLB->description());
        registerField(QStringLiteral("pgp"), ui.pgpCLB);
    }

    void setProtocol(Protocol proto)
    {
        if (proto == OpenPGP) {
            ui.pgpCLB->setChecked(true);
        } else if (proto == CMS) {
            ui.x509CLB->setChecked(true);
        } else {
            force_set_checked(ui.pgpCLB,  false);
            force_set_checked(ui.x509CLB, false);
        }
    }

    Protocol protocol() const
    {
        return
            ui.pgpCLB->isChecked()  ? OpenPGP :
            ui.x509CLB->isChecked() ? CMS : UnknownProtocol;
    }

    void initializePage() override {
        if (!initialized)
        {
            connect(ui.pgpCLB,  &QAbstractButton::clicked, wizard(), &QWizard::next, Qt::QueuedConnection);
            connect(ui.x509CLB, &QAbstractButton::clicked, wizard(), &QWizard::next, Qt::QueuedConnection);
        }
        initialized = true;
    }

    bool isComplete() const override
    {
        return protocol() != UnknownProtocol;
    }

private:
    bool initialized : 1;
    Ui_ChooseProtocolPage ui;
};

struct Line {
    QString attr;
    QString label;
    QString regex;
    QLineEdit *edit;
};

class EnterDetailsPage : public WizardPage
{
    Q_OBJECT
public:
    explicit EnterDetailsPage(QWidget *p = nullptr)
        : WizardPage(p), dialog(this), ui()
    {
        ui.setupUi(this);

        Settings settings;
        if (settings.hideAdvanced()) {
            setSubTitle(i18n("Please enter your personal details below."));
        } else {
            setSubTitle(i18n("Please enter your personal details below. If you want more control over the parameters, click on the Advanced Settings button."));
        }
        ui.advancedPB->setVisible(!settings.hideAdvanced());
        ui.resultLE->setFocusPolicy(Qt::NoFocus);

        // set errorLB to have a fixed height of two lines:
        ui.errorLB->setText(QStringLiteral("2<br>1"));
        ui.errorLB->setFixedHeight(ui.errorLB->minimumSizeHint().height());
        ui.errorLB->clear();

        connect(ui.resultLE, &QLineEdit::textChanged,
                this, &QWizardPage::completeChanged);
        // The email doesn't necessarily show up in ui.resultLE:
        connect(ui.emailLE, &QLineEdit::textChanged,
                this, &QWizardPage::completeChanged);
        registerDialogPropertiesAsFields();
        registerField(QStringLiteral("dn"), ui.resultLE);
        registerField(QStringLiteral("name"), ui.nameLE);
        registerField(QStringLiteral("email"), ui.emailLE);
        registerField(QStringLiteral("protectedKey"), ui.withPassCB);
        updateForm();
        setCommitPage(true);
        setButtonText(QWizard::CommitButton, i18nc("@action", "Create"));

        const auto conf = QGpgME::cryptoConfig();
        if (!conf) {
            qCWarning(KLEOPATRA_LOG) << "Failed to obtain cryptoConfig.";
            return;
        }
        const auto entry = getCryptoConfigEntry(conf, "gpg-agent", "enforce-passphrase-constraints");
        if (entry && entry->boolValue()) {
            qCDebug(KLEOPATRA_LOG) << "Disabling passphrace cb because of agent config.";
            ui.withPassCB->setEnabled(false);
            ui.withPassCB->setChecked(true);
        } else {
            const KConfigGroup config(KSharedConfig::openConfig(), "CertificateCreationWizard");
            ui.withPassCB->setChecked(config.readEntry("WithPassphrase", false));
            ui.withPassCB->setEnabled(!config.isEntryImmutable("WithPassphrase"));
        }
    }

    bool isComplete() const override;
    void initializePage() override {
        updateForm();
        ui.withPassCB->setVisible(pgp());
        dialog.setProtocol(pgp() ? OpenPGP : CMS);
    }
    void cleanupPage() override {
        saveValues();
        // reset protocol when navigating back to "Choose Protocol" page
        resetProtocol();
    }

private:
    void updateForm();
    void clearForm();
    void saveValues();
    void registerDialogPropertiesAsFields();

private:
    QString pgpUserID() const;
    QString cmsDN() const;

private Q_SLOTS:
    void slotAdvancedSettingsClicked();
    void slotUpdateResultLabel()
    {
        ui.resultLE->setText(pgp() ? pgpUserID() : cmsDN());
    }

private:
    QVector<Line> lineList;
    QList<QWidget *> dynamicWidgets;
    QMap<QString, QString> savedValues;
    AdvancedSettingsDialog dialog;
    Ui_EnterDetailsPage ui;
};

class KeyCreationPage : public WizardPage
{
    Q_OBJECT
public:
    explicit KeyCreationPage(QWidget *p = nullptr)
        : WizardPage(p),
          ui()
    {
        ui.setupUi(this);
    }

    bool isComplete() const override
    {
        return !job;
    }

    void initializePage() override {
        startJob();
    }

private:
    void startJob()
    {
        const auto proto = pgp() ? QGpgME::openpgp() : QGpgME::smime();
        if (!proto) {
            return;
        }
        QGpgME::KeyGenerationJob *const j = proto->keyGenerationJob();
        if (!j) {
            return;
        }
        if (!protectedKey() && pgp()) {
            auto ctx = QGpgME::Job::context(j);
            ctx->setPassphraseProvider(&mEmptyPWProvider);
            ctx->setPinentryMode(Context::PinentryLoopback);
        }
        connect(j, &QGpgME::KeyGenerationJob::result,
                this, &KeyCreationPage::slotResult);
        if (const Error err = j->start(createGnupgKeyParms()))
            setField(QStringLiteral("error"), i18n("Could not start key pair creation: %1",
                                                   QString::fromLocal8Bit(err.asString())));
        else {
            job = j;
        }
    }
    QStringList keyUsages() const;
    QStringList subkeyUsages() const;
    QString createGnupgKeyParms() const;
    EmptyPassphraseProvider mEmptyPWProvider;

private Q_SLOTS:
    void slotResult(const GpgME::KeyGenerationResult &result, const QByteArray &request, const QString &auditLog)
    {
        Q_UNUSED(auditLog)
        if (result.error().code() || (pgp() && !result.fingerprint())) {
            setField(QStringLiteral("error"), result.error().isCanceled()
                     ? i18n("Operation canceled.")
                     : i18n("Could not create key pair: %1",
                            QString::fromLocal8Bit(result.error().asString())));
            setField(QStringLiteral("url"), QString());
            setField(QStringLiteral("result"), QString());
        } else if (pgp()) {
            setField(QStringLiteral("error"), QString());
            setField(QStringLiteral("url"), QString());
            setField(QStringLiteral("result"), i18n("Key pair created successfully.\n"
                                                    "Fingerprint: %1", Formatting::prettyID(result.fingerprint())));
        } else {
            QFile file(tmpDir().absoluteFilePath(QStringLiteral("request.p10")));

            if (!file.open(QIODevice::WriteOnly)) {
                setField(QStringLiteral("error"), i18n("Could not write output file %1: %2",
                                                       file.fileName(), file.errorString()));
                setField(QStringLiteral("url"), QString());
                setField(QStringLiteral("result"), QString());
            } else {
                file.write(request);
                setField(QStringLiteral("error"), QString());
                setField(QStringLiteral("url"), QUrl::fromLocalFile(file.fileName()).toString());
                setField(QStringLiteral("result"), i18n("Key pair created successfully."));
            }
        }
        // Ensure that we have the key in the keycache
        if (pgp() && !result.error().code() && result.fingerprint()) {
            auto ctx = Context::createForProtocol(OpenPGP);
            if (ctx) {
                // Check is pretty useless something very buggy in that case.
                Error e;
                const auto key = ctx->key(result.fingerprint(), e, true);
                if (!key.isNull()) {
                    KeyCache::mutableInstance()->insert(key);
                } else {
                    qCDebug(KLEOPATRA_LOG) << "Failed to find newly generated key.";
                }
                delete ctx;
            }
        }
        setField(QStringLiteral("fingerprint"), result.fingerprint() ?
                 QString::fromLatin1(result.fingerprint()) : QString());
        job = nullptr;
        Q_EMIT completeChanged();
        const KConfigGroup config(KSharedConfig::openConfig(), "CertificateCreationWizard");
        if (config.readEntry("SkipResultPage", false)) {
            if (result.fingerprint()) {
                KleopatraApplication::instance()->slotActivateRequested(QStringList() <<
                       QStringLiteral("kleopatra") << QStringLiteral("--query") << QLatin1String(result.fingerprint()), QString());
                QMetaObject::invokeMethod(wizard(), "close", Qt::QueuedConnection);
            } else {
                QMetaObject::invokeMethod(wizard(), "next", Qt::QueuedConnection);
            }
        } else {
            QMetaObject::invokeMethod(wizard(), "next", Qt::QueuedConnection);
        }
    }

private:
    QPointer<QGpgME::KeyGenerationJob> job;
    Ui_KeyCreationPage ui;
};

class ResultPage : public WizardPage
{
    Q_OBJECT
public:
    explicit ResultPage(QWidget *p = nullptr)
        : WizardPage(p),
          initialized(false),
          successfullyCreatedSigningCertificate(false),
          successfullyCreatedEncryptionCertificate(false),
          ui()
    {
        ui.setupUi(this);
        ui.dragQueen->setPixmap(QIcon::fromTheme(QStringLiteral("kleopatra")).pixmap(64, 64));
        registerField(QStringLiteral("error"),  ui.errorTB,   "plainText");
        registerField(QStringLiteral("result"), ui.resultTB,  "plainText");
        registerField(QStringLiteral("url"),    ui.dragQueen, "url");
        // hidden field, since QWizard can't deal with non-widget-backed fields...
        auto le = new QLineEdit(this);
        le->hide();
        registerField(QStringLiteral("fingerprint"), le);
    }

    void initializePage() override {
        const bool error = isError();

        if (error)
        {
            setTitle(i18nc("@title", "Key Creation Failed"));
            setSubTitle(i18n("Key pair creation failed. Please find details about the failure below."));
        } else {
            setTitle(i18nc("@title", "Key Pair Successfully Created"));
            setSubTitle(i18n("Your new key pair was created successfully. Please find details on the result and some suggested next steps below."));
        }

        ui.resultTB                 ->setVisible(!error);
        ui.errorTB                  ->setVisible(error);
        ui.dragQueen                ->setVisible(!error &&!pgp());
        ui.restartWizardPB          ->setVisible(error);
        ui.nextStepsGB              ->setVisible(!error);
        ui.saveRequestToFilePB      ->setVisible(!pgp());
        ui.makeBackupPB             ->setVisible(pgp());
        ui.createRevocationRequestPB->setVisible(pgp() &&false);     // not implemented

        ui.sendCertificateByEMailPB ->setVisible(pgp());
        ui.sendRequestByEMailPB     ->setVisible(!pgp());
        ui.uploadToKeyserverPB      ->setVisible(pgp());

        if (!error && !pgp())
        {
            if (signingAllowed() && !encryptionAllowed()) {
                successfullyCreatedSigningCertificate = true;
            } else if (!signingAllowed() && encryptionAllowed()) {
                successfullyCreatedEncryptionCertificate = true;
            } else {
                successfullyCreatedEncryptionCertificate = successfullyCreatedSigningCertificate = true;
            }
        }

        ui.createSigningCertificatePB->setVisible(successfullyCreatedEncryptionCertificate &&!successfullyCreatedSigningCertificate);
        ui.createEncryptionCertificatePB->setVisible(successfullyCreatedSigningCertificate &&!successfullyCreatedEncryptionCertificate);

        if (error) {
            wizard()->setOptions(wizard()->options() & ~QWizard::NoCancelButtonOnLastPage);
        } else {
            wizard()->setOptions(wizard()->options() | QWizard::NoCancelButtonOnLastPage);
        }

        if (!initialized) {
            connect(ui.restartWizardPB, &QAbstractButton::clicked,
                    this, [this]() {
                        restartAtEnterDetailsPage();
                    });
        }
        initialized = true;
    }

    bool isError() const
    {
        return !ui.errorTB->document()->isEmpty();
    }

    bool isComplete() const override
    {
        return !isError();
    }

private:
    Key key() const
    {
        return KeyCache::instance()->findByFingerprint(fingerprint().toLatin1().constData());
    }

private Q_SLOTS:
    void slotSaveRequestToFile()
    {
        QString fileName = FileDialog::getSaveFileName(this, i18nc("@title", "Save Request"),
                           QStringLiteral("imp"), i18n("PKCS#10 Requests (*.p10)"));
        if (fileName.isEmpty()) {
            return;
        }
        if (!fileName.endsWith(QLatin1String(".p10"), Qt::CaseInsensitive)) {
            fileName += QLatin1String(".p10");
        }
        QFile src(QUrl(url()).toLocalFile());
        if (!src.copy(fileName))
            KMessageBox::error(this,
                               xi18nc("@info",
                                      "Could not copy temporary file <filename>%1</filename> "
                                      "to file <filename>%2</filename>: <message>%3</message>",
                                      src.fileName(), fileName, src.errorString()),
                               i18nc("@title", "Error Saving Request"));
        else
            KMessageBox::information(this,
                                     xi18nc("@info",
                                            "<para>Successfully wrote request to <filename>%1</filename>.</para>"
                                            "<para>You should now send the request to the Certification Authority (CA).</para>",
                                            fileName),
                                     i18nc("@title", "Request Saved"));
    }

    void slotSendRequestByEMail()
    {
        if (pgp()) {
            return;
        }
        const KConfigGroup config(KSharedConfig::openConfig(), "CertificateCreationWizard");
        invokeMailer(config.readEntry("CAEmailAddress"),    // to
                     i18n("Please process this certificate."), // subject
                     i18n("Please process this certificate and inform the sender about the location to fetch the resulting certificate.\n\nThanks,\n"), // body
                     QUrl(url()).toLocalFile());    // attachment
    }

    void slotSendCertificateByEMail()
    {
        if (!pgp() || exportCertificateCommand) {
            return;
        }
        auto cmd = new ExportCertificateCommand(key());
        connect(cmd, &ExportCertificateCommand::finished, this, &ResultPage::slotSendCertificateByEMailContinuation);
        cmd->setOpenPGPFileName(tmpDir().absoluteFilePath(fingerprint() + QLatin1String(".asc")));
        cmd->start();
        exportCertificateCommand = cmd;
    }

    void slotSendCertificateByEMailContinuation()
    {
        if (!exportCertificateCommand) {
            return;
        }
        // ### better error handling?
        const QString fileName = exportCertificateCommand->openPGPFileName();
        qCDebug(KLEOPATRA_LOG) << "fileName" << fileName;
        exportCertificateCommand = nullptr;
        if (fileName.isEmpty()) {
            return;
        }
        invokeMailer(QString(),  // to
                     i18n("My new public OpenPGP key"), // subject
                     i18n("Please find attached my new public OpenPGP key."), // body
                     fileName);
    }

    QByteArray ol_quote(QByteArray str)
    {
#ifdef Q_OS_WIN
        return "\"\"" + str.replace('"', "\\\"") + "\"\"";
        //return '"' + str.replace( '"', "\\\"" ) + '"';
#else
        return str;
#endif
    }

    void invokeMailer(const QString &to, const QString &subject, const QString &body, const QString &attachment)
    {
        qCDebug(KLEOPATRA_LOG) << "to:" << to << "subject:" << subject
                               << "body:" << body << "attachment:" << attachment;

        // RFC 2368 says body's linebreaks need to be encoded as
        // "%0D%0A", so normalize body to CRLF:
        //body.replace(QLatin1Char('\n'), QStringLiteral("\r\n")).remove(QStringLiteral("\r\r"));

        QUrlQuery query;
        query.addQueryItem(QStringLiteral("subject"), subject);
        query.addQueryItem(QStringLiteral("body"), body);
        if (!attachment.isEmpty()) {
            query.addQueryItem(QStringLiteral("attach"), attachment);
        }
        QUrl url;
        url.setScheme(QStringLiteral("mailto"));
        url.setQuery(query);
        qCDebug(KLEOPATRA_LOG) << "openUrl" << url;
        QDesktopServices::openUrl(url);
        KMessageBox::information(this,
                                 xi18nc("@info",
                                        "<para><application>Kleopatra</application> tried to send a mail via your default mail client.</para>"
                                        "<para>Some mail clients are known not to support attachments when invoked this way.</para>"
                                        "<para>If your mail client does not have an attachment, then drag the <application>Kleopatra</application> icon and drop it on the message compose window of your mail client.</para>"
                                        "<para>If that does not work, either, save the request to a file, and then attach that.</para>"),
                                 i18nc("@title", "Sending Mail"),
                                 QStringLiteral("newcertificatewizard-mailto-troubles"));
    }

    void slotUploadCertificateToDirectoryServer()
    {
        if (pgp()) {
            (new ExportOpenPGPCertsToServerCommand(key()))->start();
        }
    }

    void slotBackupCertificate()
    {
        if (pgp()) {
            (new ExportSecretKeyCommand(key()))->start();
        }
    }

    void slotCreateRevocationRequest()
    {

    }

    void slotCreateSigningCertificate()
    {
        if (successfullyCreatedSigningCertificate) {
            return;
        }
        toggleSignEncryptAndRestart();
    }

    void slotCreateEncryptionCertificate()
    {
        if (successfullyCreatedEncryptionCertificate) {
            return;
        }
        toggleSignEncryptAndRestart();
    }

private:
    void toggleSignEncryptAndRestart()
    {
        if (!wizard()) {
            return;
        }
        if (KMessageBox::warningContinueCancel(
                    this,
                    i18nc("@info",
                          "This operation will delete the certification request. "
                          "Please make sure that you have sent or saved it before proceeding."),
                    i18nc("@title", "Certification Request About To Be Deleted")) != KMessageBox::Continue) {
            return;
        }
        const bool sign = signingAllowed();
        const bool encr = encryptionAllowed();
        setField(QStringLiteral("signingAllowed"),    !sign);
        setField(QStringLiteral("encryptionAllowed"), !encr);
        restartAtEnterDetailsPage();
    }

private:
    bool initialized : 1;
    bool successfullyCreatedSigningCertificate : 1;
    bool successfullyCreatedEncryptionCertificate : 1;
    QPointer<ExportCertificateCommand> exportCertificateCommand;
    Ui_ResultPage ui;
};
}

class NewCertificateWizard::Private
{
    friend class ::Kleo::NewCertificateWizard;
    friend class ::Kleo::NewCertificateUi::WizardPage;
    NewCertificateWizard *const q;
public:
    explicit Private(NewCertificateWizard *qq)
        : q(qq),
          tmp(QDir::temp().absoluteFilePath(QStringLiteral("kleo-"))),
          ui(q)
    {
        q->setWindowTitle(i18nc("@title:window", "Key Pair Creation Wizard"));
    }

private:
    GpgME::Protocol initialProtocol = GpgME::UnknownProtocol;
    QTemporaryDir tmp;
    struct Ui {
        ChooseProtocolPage chooseProtocolPage;
        EnterDetailsPage enterDetailsPage;
        KeyCreationPage keyCreationPage;
        ResultPage resultPage;

        explicit Ui(NewCertificateWizard *q)
            : chooseProtocolPage(q),
              enterDetailsPage(q),
              keyCreationPage(q),
              resultPage(q)
        {
            KDAB_SET_OBJECT_NAME(chooseProtocolPage);
            KDAB_SET_OBJECT_NAME(enterDetailsPage);
            KDAB_SET_OBJECT_NAME(keyCreationPage);
            KDAB_SET_OBJECT_NAME(resultPage);

            q->setOptions(NoBackButtonOnStartPage|DisabledBackButtonOnLastPage);

            q->setPage(ChooseProtocolPageId, &chooseProtocolPage);
            q->setPage(EnterDetailsPageId,   &enterDetailsPage);
            q->setPage(KeyCreationPageId,    &keyCreationPage);
            q->setPage(ResultPageId,         &resultPage);

            q->setStartId(ChooseProtocolPageId);
        }

    } ui;

};

NewCertificateWizard::NewCertificateWizard(QWidget *p)
    : QWizard(p), d(new Private(this))
{
}

NewCertificateWizard::~NewCertificateWizard() {}

void NewCertificateWizard::showEvent(QShowEvent *event)
{
    // set WA_KeyboardFocusChange attribute to force visual focus of the
    // focussed button when the wizard is shown (required for Breeze style
    // and some other styles)
    window()->setAttribute(Qt::WA_KeyboardFocusChange);
    QWizard::showEvent(event);
}

void NewCertificateWizard::setProtocol(Protocol proto)
{
    d->initialProtocol = proto;
    d->ui.chooseProtocolPage.setProtocol(proto);
    setStartId(proto == UnknownProtocol ? ChooseProtocolPageId : EnterDetailsPageId);
}

Protocol NewCertificateWizard::protocol() const
{
    return d->ui.chooseProtocolPage.protocol();
}

void NewCertificateWizard::resetProtocol()
{
    d->ui.chooseProtocolPage.setProtocol(d->initialProtocol);
}

void NewCertificateWizard::restartAtEnterDetailsPage()
{
    const auto protocol = d->ui.chooseProtocolPage.protocol();
    restart();  // resets the protocol to the initial protocol
    d->ui.chooseProtocolPage.setProtocol(protocol);
    while (currentId() != NewCertificateWizard::EnterDetailsPageId) {
        next();
    }
}

static QString pgpLabel(const QString &attr)
{
    if (attr == QLatin1String("NAME")) {
        return i18n("Name");
    }
    if (attr == QLatin1String("EMAIL")) {
        return i18n("EMail");
    }
    return QString();
}

static QString attributeLabel(const QString &attr, bool pgp)
{
    if (attr.isEmpty()) {
        return QString();
    }
    const QString label = pgp ? pgpLabel(attr) : Kleo::DN::attributeNameToLabel(attr);
    if (!label.isEmpty())
        if (pgp) {
            return label;
        } else
            return i18nc("Format string for the labels in the \"Your Personal Data\" page",
                         "%1 (%2)", label, attr);
    else {
        return attr;
    }
}

#if 0
//Not used anywhere
static QString attributeLabelWithColor(const QString &attr, bool pgp)
{
    const QString result = attributeLabel(attr, pgp);
    if (result.isEmpty()) {
        return QString();
    } else {
        return result + ':';
    }
}
#endif

static QString attributeFromKey(QString key)
{
    return key.remove(QLatin1Char('!'));
}

QDir WizardPage::tmpDir() const
{
    return wizard() ? QDir(wizard()->d->tmp.path()) : QDir::home();
}

void EnterDetailsPage::registerDialogPropertiesAsFields()
{

    const QMetaObject *const mo = dialog.metaObject();
    for (unsigned int i = mo->propertyOffset(), end = i + mo->propertyCount(); i != end; ++i) {
        const QMetaProperty mp = mo->property(i);
        if (mp.isValid()) {
            registerField(QLatin1String(mp.name()), &dialog, mp.name(), SIGNAL(accepted()));
        }
    }

}

void EnterDetailsPage::saveValues()
{
    for (const Line &line : std::as_const(lineList)) {
        savedValues[ attributeFromKey(line.attr) ] = line.edit->text().trimmed();
    }
}

void EnterDetailsPage::clearForm()
{
    qDeleteAll(dynamicWidgets);
    dynamicWidgets.clear();
    lineList.clear();

    ui.nameLE->hide();
    ui.nameLE->clear();
    ui.nameLB->hide();
    ui.nameRequiredLB->hide();

    ui.emailLE->hide();
    ui.emailLE->clear();
    ui.emailLB->hide();
    ui.emailRequiredLB->hide();
}

static int row_index_of(QWidget *w, QGridLayout *l)
{
    const int idx = l->indexOf(w);
    int r, c, rs, cs;
    l->getItemPosition(idx, &r, &c, &rs, &cs);
    return r;
}

static QLineEdit *adjust_row(QGridLayout *l, int row, const QString &label, const QString &preset, QValidator *validator, bool readonly, bool required)
{
    Q_ASSERT(l);
    Q_ASSERT(row >= 0);
    Q_ASSERT(row < l->rowCount());

    auto lb = qobject_cast<QLabel *>(l->itemAtPosition(row, 0)->widget());
    Q_ASSERT(lb);
    auto le = qobject_cast<QLineEdit *>(l->itemAtPosition(row, 1)->widget());
    Q_ASSERT(le);
    lb->setBuddy(le);   // For better accessibility
    auto reqLB = qobject_cast<QLabel *>(l->itemAtPosition(row, 2)->widget());
    Q_ASSERT(reqLB);

    lb->setText(i18nc("interpunctation for labels", "%1:", label));
    le->setText(preset);
    reqLB->setText(required ? i18n("(required)") : i18n("(optional)"));
    delete le->validator();
    if (validator) {
        if (!validator->parent()) {
            validator->setParent(le);
        }
        le->setValidator(validator);
    }

    le->setReadOnly(readonly && le->hasAcceptableInput());

    lb->show();
    le->show();
    reqLB->show();

    return le;
}

static int add_row(QGridLayout *l, QList<QWidget *> *wl)
{
    Q_ASSERT(l);
    Q_ASSERT(wl);
    const int row = l->rowCount();
    QWidget *w1, *w2, *w3;
    l->addWidget(w1 = new QLabel(l->parentWidget()),    row, 0);
    l->addWidget(w2 = new QLineEdit(l->parentWidget()), row, 1);
    l->addWidget(w3 = new QLabel(l->parentWidget()),    row, 2);
    wl->push_back(w1);
    wl->push_back(w2);
    wl->push_back(w3);
    return row;
}

void EnterDetailsPage::updateForm()
{

    clearForm();

    const auto settings = Kleo::Settings{};
    const KConfigGroup config(KSharedConfig::openConfig(), "CertificateCreationWizard");

    QStringList attrOrder = config.readEntry(pgp() ? "OpenPGPAttributeOrder" : "DNAttributeOrder", QStringList());
    if (attrOrder.empty()) {
        if (pgp()) {
            attrOrder << QStringLiteral("NAME") << QStringLiteral("EMAIL");
        } else {
            attrOrder << QStringLiteral("CN!") << QStringLiteral("L") << QStringLiteral("OU") << QStringLiteral("O") << QStringLiteral("C") << QStringLiteral("EMAIL!");
        }
    }

    QList<QWidget *> widgets;
    widgets.push_back(ui.nameLE);
    widgets.push_back(ui.emailLE);

    QMap<int, Line> lines;

    for (const QString &rawKey : std::as_const(attrOrder)) {
        const QString key = rawKey.trimmed().toUpper();
        const QString attr = attributeFromKey(key);
        if (attr.isEmpty()) {
            continue;
        }
        const QString preset = savedValues.value(attr, config.readEntry(attr, QString()));
        const bool required = key.endsWith(QLatin1Char('!'));
        const bool readonly = config.isEntryImmutable(attr);
        const QString label = config.readEntry(attr + QLatin1String("_label"),
                                               attributeLabel(attr, pgp()));
        const QString regex = config.readEntry(attr + QLatin1String("_regex"));
        const QString placeholder = config.readEntry(attr + QLatin1String{"_placeholder"});

        int row;
        bool known = true;
        QValidator *validator = nullptr;
        if (attr == QLatin1String("EMAIL")) {
            row = row_index_of(ui.emailLE, ui.gridLayout);
            validator = regex.isEmpty() ? Validation::email() : Validation::email(regex);
        } else if (attr == QLatin1String("NAME") || attr == QLatin1String("CN")) {
            if ((pgp() && attr == QLatin1String("CN")) || (!pgp() && attr == QLatin1String("NAME"))) {
                continue;
            }
            if (pgp()) {
                validator = regex.isEmpty() ? Validation::pgpName() : Validation::pgpName(regex);
            }
            row = row_index_of(ui.nameLE, ui.gridLayout);
        } else {
            known = false;
            row = add_row(ui.gridLayout, &dynamicWidgets);
        }
        if (!validator && !regex.isEmpty()) {
            validator = new QRegularExpressionValidator{QRegularExpression{regex}, nullptr};
        }

        QLineEdit *le = adjust_row(ui.gridLayout, row, label, preset, validator, readonly, required);
        le->setPlaceholderText(placeholder);

        const Line line = { key, label, regex, le };
        lines[row] = line;

        if (!known) {
            widgets.push_back(le);
        }

        // don't connect twice:
        disconnect(le, &QLineEdit::textChanged, this, &EnterDetailsPage::slotUpdateResultLabel);
        connect(le, &QLineEdit::textChanged, this, &EnterDetailsPage::slotUpdateResultLabel);
    }

    // create lineList in visual order, so requirementsAreMet()
    // complains from top to bottom:
    lineList.reserve(lines.count());
    std::copy(lines.cbegin(), lines.cend(), std::back_inserter(lineList));

    widgets.push_back(ui.withPassCB);
    widgets.push_back(ui.advancedPB);

    const bool prefillName = (pgp() && settings.prefillName()) || (!pgp() && settings.prefillCN());
    if (ui.nameLE->text().isEmpty() && prefillName) {
        ui.nameLE->setText(userFullName());
    }
    if (ui.emailLE->text().isEmpty() && settings.prefillEmail()) {
        ui.emailLE->setText(userEmailAddress());
    }

    slotUpdateResultLabel();

    set_tab_order(widgets);
}

QString EnterDetailsPage::cmsDN() const
{
    DN dn;
    for (QVector<Line>::const_iterator it = lineList.begin(), end = lineList.end(); it != end; ++it) {
        const QString text = it->edit->text().trimmed();
        if (text.isEmpty()) {
            continue;
        }
        QString attr = attributeFromKey(it->attr);
        if (attr == QLatin1String("EMAIL")) {
            continue;
        }
        if (const char *const oid = oidForAttributeName(attr)) {
            attr = QString::fromUtf8(oid);
        }
        dn.append(DN::Attribute(attr, text));
    }
    return dn.dn();
}

QString EnterDetailsPage::pgpUserID() const
{
    return Formatting::prettyNameAndEMail(OpenPGP, QString(),
                                          ui.nameLE->text().trimmed(),
                                          ui.emailLE->text().trimmed(),
                                          QString());
}

static bool has_intermediate_input(const QLineEdit *le)
{
    QString text = le->text();
    int pos = le->cursorPosition();
    const QValidator *const v = le->validator();
    return v && v->validate(text, pos) == QValidator::Intermediate;
}

static bool requirementsAreMet(const QVector<Line> &list, QString &error)
{
    bool allEmpty = true;
    for (const Line &line : list) {
        const QLineEdit *le = line.edit;
        if (!le) {
            continue;
        }
        const QString key = line.attr;
        qCDebug(KLEOPATRA_LOG) << "requirementsAreMet(): checking" << key << "against" << le->text() << ":";
        if (le->text().trimmed().isEmpty()) {
            if (key.endsWith(QLatin1Char('!'))) {
                if (line.regex.isEmpty()) {
                    error = xi18nc("@info", "<interface>%1</interface> is required, but empty.", line.label);
                } else
                    error = xi18nc("@info", "<interface>%1</interface> is required, but empty.<nl/>"
                                   "Local Admin rule: <icode>%2</icode>", line.label, line.regex);
                return false;
            }
        } else if (has_intermediate_input(le)) {
            if (line.regex.isEmpty()) {
                error = xi18nc("@info", "<interface>%1</interface> is incomplete.", line.label);
            } else
                error = xi18nc("@info", "<interface>%1</interface> is incomplete.<nl/>"
                               "Local Admin rule: <icode>%2</icode>", line.label, line.regex);
            return false;
        } else if (!le->hasAcceptableInput()) {
            if (line.regex.isEmpty()) {
                error = xi18nc("@info", "<interface>%1</interface> is invalid.", line.label);
            } else
                error = xi18nc("@info", "<interface>%1</interface> is invalid.<nl/>"
                               "Local Admin rule: <icode>%2</icode>", line.label, line.regex);
            return false;
        } else {
            allEmpty = false;
        }
    }
    // Ensure that at least one value is acceptable
    return !allEmpty;
}

bool EnterDetailsPage::isComplete() const
{
    QString error;
    const bool ok = requirementsAreMet(lineList, error);
    ui.errorLB->setText(error);
    return ok;
}

void EnterDetailsPage::slotAdvancedSettingsClicked()
{
    dialog.exec();
}

QStringList KeyCreationPage::keyUsages() const
{
    QStringList usages;
    if (signingAllowed()) {
        usages << QStringLiteral("sign");
    }
    if (encryptionAllowed() && !is_ecdh(subkeyType()) &&
        !is_dsa(keyType()) && !is_rsa(subkeyType())) {
        usages << QStringLiteral("encrypt");
    }
    if (authenticationAllowed()) {
        usages << QStringLiteral("auth");
    }
    if (usages.empty() && certificationAllowed()) {
        /* Empty usages cause an error so we need to
         * add at least certify if nothing else is selected */
        usages << QStringLiteral("cert");
    }
    return usages;
}

QStringList KeyCreationPage::subkeyUsages() const
{
    QStringList usages;
    if (encryptionAllowed() && (is_dsa(keyType()) || is_rsa(subkeyType()) ||
                                is_ecdh(subkeyType()))) {
        Q_ASSERT(subkeyType());
        usages << QStringLiteral("encrypt");
    }
    return usages;
}

namespace
{
template <typename T = QString>
struct Row {
    QString key;
    T value;

    Row(const QString &k, const T &v) : key(k), value(v) {}
};
template <typename T>
QTextStream &operator<<(QTextStream &s, const Row<T> &row)
{
    if (row.key.isEmpty()) {
        return s;
    } else {
        return s << "<tr><td>" << row.key << "</td><td>" << row.value << "</td></tr>";
    }
}
}

QString KeyCreationPage::createGnupgKeyParms() const
{
    KeyParameters keyParameters(pgp() ? KeyParameters::OpenPGP : KeyParameters::CMS);

    keyParameters.setKeyType(keyType());
    if (is_ecdsa(keyType()) || is_eddsa(keyType())) {
        keyParameters.setKeyCurve(keyCurve());
    } else if (const unsigned int strength = keyStrength()) {
        keyParameters.setKeyLength(strength);
    }
    keyParameters.setKeyUsages(keyUsages());

    if (subkeyType()) {
        keyParameters.setSubkeyType(subkeyType());
        if (is_ecdh(subkeyType())) {
            keyParameters.setSubkeyCurve(subkeyCurve());
        } else if (const unsigned int strength = subkeyStrength()) {
            keyParameters.setSubkeyLength(strength);
        }
        keyParameters.setSubkeyUsages(subkeyUsages());
    }

    if (pgp()) {
        if (expiryDate().isValid()) {
            keyParameters.setExpirationDate(expiryDate());
        }
        if (!name().isEmpty()) {
            keyParameters.setName(name());
        }
        if (!email().isEmpty()) {
            keyParameters.setEmail(email());
        }
    } else {
        keyParameters.setDN(dn());
        keyParameters.setEmail(email());
        const auto addesses{additionalEMailAddresses()};
        for (const QString &email : addesses) {
            keyParameters.addEmail(email);
        }
        const auto dnsN{dnsNames()};
        for (const QString &dns : dnsN) {
            keyParameters.addDomainName(dns);
        }
        const auto urisList{uris()};
        for (const QString &uri : urisList) {
            keyParameters.addURI(uri);
        }
    }

    const QString result = keyParameters.toString();
    qCDebug(KLEOPATRA_LOG) << '\n' << result;
    return result;
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

    fill_combobox(*ui.rsaKeyStrengthCB, rsaKeySizes, rsaKeySizeLabels);
    fill_combobox(*ui.rsaKeyStrengthSubCB, rsaKeySizes, rsaKeySizeLabels);
    fill_combobox(*ui.dsaKeyStrengthCB, dsaKeySizes, dsaKeySizeLabels);
    fill_combobox(*ui.elgKeyStrengthCB, elgKeySizes, elgKeySizeLabels);
    if (mEdDSASupported) {
        // If supported we recommend cv25519
        ui.ecdsaKeyCurvesCB->addItem(QStringLiteral("ed25519"));
        ui.ecdhKeyCurvesCB->addItem(QStringLiteral("cv25519"));
    }
    ui.ecdhKeyCurvesCB->addItems(curveNames);
    ui.ecdsaKeyCurvesCB->addItems(curveNames);
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
        if (ui.tabWidget->indexOf(ui.personalTab) != -1) {
            ui.tabWidget->removeTab(ui.tabWidget->indexOf(ui.personalTab));
        }
    } else {
        if (ui.tabWidget->indexOf(ui.personalTab) == -1) {
            ui.tabWidget->addTab(ui.personalTab, tr2i18n("Personal Details", nullptr));
        }
    }
    ui.uidGB->setVisible(protocol == OpenPGP);
    ui.uidGB->setEnabled(false);
    ui.uidGB->setToolTip(i18nc("@info:tooltip", "Adding more than one user ID is not yet implemented."));
    ui.emailGB->setVisible(protocol == CMS);
    ui.dnsGB->setVisible(protocol == CMS);
    ui.uriGB->setVisible(protocol == CMS);

    // Technical Details Page
    ui.ecdhCB->setVisible(mECCSupported);
    ui.ecdhKeyCurvesCB->setVisible(mECCSupported);
    ui.ecdsaKeyCurvesCB->setVisible(mECCSupported);
    ui.ecdsaRB->setVisible(mECCSupported);
    if (mEdDSASupported) {
        // We use the same radio button for EdDSA as we use for
        // ECDSA GnuPG does the same and this is really super technical
        // land.
        ui.ecdsaRB->setText(QStringLiteral("ECDSA/EdDSA"));
    }

    const bool deVsHack = Kleo::gnupgUsesDeVsCompliance();

    if (deVsHack) {
        // GnuPG Provides no API to query which keys are compliant for
        // a mode. If we request a different one it will error out so
        // we have to remove the options.
        //
        // Does anyone want to use NIST anyway?
        int i;
        while ((i = ui.ecdsaKeyCurvesCB->findText(QStringLiteral("NIST"), Qt::MatchStartsWith)) != -1 ||
               (i = ui.ecdsaKeyCurvesCB->findText(QStringLiteral("25519"), Qt::MatchEndsWith)) != -1) {
            ui.ecdsaKeyCurvesCB->removeItem(i);
        }
        while ((i = ui.ecdhKeyCurvesCB->findText(QStringLiteral("NIST"), Qt::MatchStartsWith)) != -1 ||
               (i = ui.ecdhKeyCurvesCB->findText(QStringLiteral("25519"), Qt::MatchEndsWith)) != -1) {
            ui.ecdhKeyCurvesCB->removeItem(i);
        }
    }
    ui.certificationCB->setVisible(protocol == OpenPGP);   // gpgsm limitation?
    ui.authenticationCB->setVisible(protocol == OpenPGP);

    if (keyTypeImmutable) {
        ui.rsaRB->setEnabled(false);
        ui.rsaSubCB->setEnabled(false);
        ui.dsaRB->setEnabled(false);
        ui.elgCB->setEnabled(false);
        ui.ecdsaRB->setEnabled(false);
        ui.ecdhCB->setEnabled(false);

        // force usage if key type is forced
        ui.certificationCB->setEnabled(false);
        ui.signingCB->setEnabled(false);
        ui.encryptionCB->setEnabled(false);
        ui.authenticationCB->setEnabled(false);
    } else {
        ui.rsaRB->setEnabled(true);
        ui.rsaSubCB->setEnabled(protocol == OpenPGP);
        ui.dsaRB->setEnabled(protocol == OpenPGP && !deVsHack);
        ui.elgCB->setEnabled(protocol == OpenPGP && !deVsHack);
        ui.ecdsaRB->setEnabled(protocol == OpenPGP);
        ui.ecdhCB->setEnabled(protocol == OpenPGP);

        if (protocol == OpenPGP) {
            // OpenPGP keys must have certify capability
            ui.certificationCB->setEnabled(false);
        }
        if (protocol == CMS) {
            ui.encryptionCB->setEnabled(true);
            ui.rsaKeyStrengthSubCB->setEnabled(false);
        }
    }
    if (protocol == OpenPGP) {
        // OpenPGP keys must have certify capability
        ui.certificationCB->setChecked(true);
    }
    if (protocol == CMS) {
        ui.rsaSubCB->setChecked(false);
    }

    ui.expiryDE->setVisible(protocol == OpenPGP);
    ui.expiryCB->setVisible(protocol == OpenPGP);

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

bool focusFirstCheckedButton(const std::vector<QAbstractButton *> &buttons)
{
    return focusFirstButtonIf(buttons,
                              [](auto btn) {
                                  return btn && btn->isEnabled() && btn->isChecked();
                              });
}

bool focusFirstEnabledButton(const std::vector<QAbstractButton *> &buttons)
{
    return focusFirstButtonIf(buttons,
                              [](auto btn) {
                                  return btn && btn->isEnabled();
                              });
}

void AdvancedSettingsDialog::setInitialFocus()
{
    // first try the key type radio buttons
    if (focusFirstCheckedButton({ui.rsaRB, ui.dsaRB, ui.ecdsaRB})) {
        return;
    }
    // then try the usage check boxes and the expiration check box
    if (focusFirstEnabledButton({ui.signingCB, ui.certificationCB, ui.encryptionCB, ui.authenticationCB, ui.expiryCB})) {
        return;
    }
    // finally, focus the OK button
    ui.buttonBox->button(QDialogButtonBox::Ok)->setFocus();
}

#include "newcertificatewizard.moc"
