/*  crypto/gui/signencryptwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "signencryptwidget.h"

#include "kleopatra_debug.h"

#include "certificatelineedit.h"
#include "fileoperationspreferences.h"
#include "kleopatraapplication.h"
#include "settings.h"
#include "unknownrecipientwidget.h"

#include "commands/detailscommand.h"

#include "dialogs/certificateselectiondialog.h"
#include "dialogs/groupdetailsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QScrollBar>

#include <Libkleo/DefaultKeyFilter>
#include <Libkleo/KeyCache>
#include <Libkleo/KeyGroup>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeySelectionCombo>
#include <Libkleo/KeyListSortFilterProxyModel>

#include <Libkleo/GnuPG>

#include <KLocalizedString>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KMessageBox>

using namespace Kleo;
using namespace Kleo::Dialogs;
using namespace GpgME;

namespace {
class SignCertificateFilter: public DefaultKeyFilter
{
public:
    SignCertificateFilter(GpgME::Protocol proto) : DefaultKeyFilter()
    {
        setRevoked(DefaultKeyFilter::NotSet);
        setExpired(DefaultKeyFilter::NotSet);
        setHasSecret(DefaultKeyFilter::Set);
        setCanSign(DefaultKeyFilter::Set);

        if (proto == GpgME::OpenPGP) {
            setIsOpenPGP(DefaultKeyFilter::Set);
        } else if (proto == GpgME::CMS) {
            setIsOpenPGP(DefaultKeyFilter::NotSet);
        }
    }
};
class EncryptCertificateFilter: public DefaultKeyFilter
{
public:
    EncryptCertificateFilter(GpgME::Protocol proto): DefaultKeyFilter()
    {
        setRevoked(DefaultKeyFilter::NotSet);
        setExpired(DefaultKeyFilter::NotSet);
        setCanEncrypt(DefaultKeyFilter::Set);

        if (proto == GpgME::OpenPGP) {
            setIsOpenPGP(DefaultKeyFilter::Set);
        } else if (proto == GpgME::CMS) {
            setIsOpenPGP(DefaultKeyFilter::NotSet);
        }
    }
};
class EncryptSelfCertificateFilter: public EncryptCertificateFilter
{
public:
    EncryptSelfCertificateFilter(GpgME::Protocol proto): EncryptCertificateFilter(proto)
    {
        setRevoked(DefaultKeyFilter::NotSet);
        setExpired(DefaultKeyFilter::NotSet);
        setCanEncrypt(DefaultKeyFilter::Set);
        setHasSecret(DefaultKeyFilter::Set);
    }
};
}

SignEncryptWidget::SignEncryptWidget(QWidget *parent, bool sigEncExclusive)
    : QWidget(parent),
      mModel(AbstractKeyListModel::createFlatKeyListModel(this)),
      mIsExclusive(sigEncExclusive)
{
    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    mModel->useKeyCache(true, KeyList::IncludeGroups);

    const bool haveSecretKeys = !KeyCache::instance()->secretKeys().empty();
    const bool havePublicKeys = !KeyCache::instance()->keys().empty();
    const bool symmetricOnly = FileOperationsPreferences().symmetricEncryptionOnly();

    /* The signature selection */
    auto sigLay = new QHBoxLayout;
    auto sigGrp = new QGroupBox(i18nc("@title:group", "Prove authenticity (sign)"));
    mSigChk = new QCheckBox(i18n("Sign as:"));
    mSigChk->setEnabled(haveSecretKeys);
    mSigChk->setChecked(haveSecretKeys);

    mSigSelect = new KeySelectionCombo();
    mSigSelect->setEnabled(mSigChk->isChecked());

    sigLay->addWidget(mSigChk);
    sigLay->addWidget(mSigSelect, 1);
    sigGrp->setLayout(sigLay);
    lay->addWidget(sigGrp);

    connect(mSigChk, &QCheckBox::toggled, mSigSelect, &QWidget::setEnabled);
    connect(mSigChk, &QCheckBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(mSigSelect, &KeySelectionCombo::currentKeyChanged,
            this, &SignEncryptWidget::updateOp);

    // Recipient selection
    auto encBoxLay = new QVBoxLayout;
    auto encBox = new QGroupBox(i18nc("@title:group", "Encrypt"));
    encBox->setLayout(encBoxLay);
    auto recipientGrid = new QGridLayout;

    // Own key
    mEncSelfChk = new QCheckBox(i18n("Encrypt for me:"));
    mEncSelfChk->setEnabled(haveSecretKeys && !symmetricOnly);
    mEncSelfChk->setChecked(haveSecretKeys && !symmetricOnly);
    mSelfSelect = new KeySelectionCombo();
    mSelfSelect->setEnabled(mEncSelfChk->isChecked());
    recipientGrid->addWidget(mEncSelfChk, 0, 0);
    recipientGrid->addWidget(mSelfSelect, 0, 1);

    // Checkbox for other keys
    mEncOtherChk = new QCheckBox(i18n("Encrypt for others:"));
    mEncOtherChk->setEnabled(havePublicKeys && !symmetricOnly);
    mEncOtherChk->setChecked(havePublicKeys && !symmetricOnly);
    recipientGrid->addWidget(mEncOtherChk, 1, 0, Qt::AlignTop);
    connect(mEncOtherChk, &QCheckBox::toggled, this,
        [this](bool toggled) {
            for (CertificateLineEdit *edit : std::as_const(mRecpWidgets)) {
                edit->setEnabled(toggled);
            }
            updateOp();
        });
    mRecpLayout = new QVBoxLayout;
    recipientGrid->addLayout(mRecpLayout, 1, 1);
    recipientGrid->setRowStretch(2, 1);

    // Scroll area for other keys
    auto recipientWidget = new QWidget;
    auto recipientScroll = new QScrollArea;
    recipientWidget->setLayout(recipientGrid);
    recipientScroll->setWidget(recipientWidget);
    recipientScroll->setWidgetResizable(true);
    recipientScroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
    recipientScroll->setFrameStyle(QFrame::NoFrame);
    recipientScroll->setFocusPolicy(Qt::NoFocus);
    recipientGrid->setContentsMargins(0, 0, 0, 0);
    encBoxLay->addWidget(recipientScroll, 1);

    auto bar = recipientScroll->verticalScrollBar();
    connect (bar, &QScrollBar::rangeChanged, this, [bar] (int, int max) {
            bar->setValue(max);
        });

    addRecipientWidget();

    // Checkbox for password
    mSymmetric = new QCheckBox(i18n("Encrypt with password. Anyone you share the password with can read the data."));
    mSymmetric->setToolTip(i18nc("Tooltip information for symmetric encryption",
                                 "Additionally to the keys of the recipients you can encrypt your data with a password. "
                                 "Anyone who has the password can read the data without any secret key. "
                                 "Using a password is <b>less secure</b> then public key cryptography. Even if you pick a very strong password."));
    mSymmetric->setChecked(symmetricOnly || !havePublicKeys);
    encBoxLay->addWidget(mSymmetric);

    // Connect it
    connect(mEncSelfChk, &QCheckBox::toggled, mSelfSelect, &QWidget::setEnabled);
    connect(mEncSelfChk, &QCheckBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(mSymmetric, &QCheckBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(mSelfSelect, &KeySelectionCombo::currentKeyChanged,
            this, &SignEncryptWidget::updateOp);

    if (mIsExclusive) {
        connect(mEncOtherChk, &QCheckBox::toggled, this, [this](bool value) {
            if (mCurrentProto != GpgME::CMS) {
                return;
            }
            if (value) {
                mSigChk->setChecked(false);
            }
        });
        connect(mEncSelfChk, &QCheckBox::toggled, this, [this](bool value) {
            if (mCurrentProto != GpgME::CMS) {
                return;
            }
            if (value) {
                mSigChk->setChecked(false);
            }
        });
        connect(mSigChk, &QCheckBox::toggled, this, [this](bool value) {
            if (mCurrentProto != GpgME::CMS) {
                return;
            }
            if (value) {
                mEncSelfChk->setChecked(false);
                mEncOtherChk->setChecked(false);
            }
        });
    }

    // Ensure that the mSigChk is aligned togehter with the encryption check boxes.
    mSigChk->setMinimumWidth(qMax(mEncOtherChk->width(), mEncSelfChk->width()));

    lay->addWidget(encBox);

    connect(KeyCache::instance().get(), &Kleo::KeyCache::keysMayHaveChanged,
            this, &SignEncryptWidget::updateCheckBoxes);
    connect(KleopatraApplication::instance(), &KleopatraApplication::configurationChanged,
            this, &SignEncryptWidget::updateCheckBoxes);

    loadKeys();
    onProtocolChanged();
    updateOp();
}

void SignEncryptWidget::setSignAsText(const QString &text)
{
    mSigChk->setText(text);
}

void SignEncryptWidget::setEncryptForMeText(const QString &text)
{
    mEncSelfChk->setText(text);
}

void SignEncryptWidget::setEncryptForOthersText(const QString &text)
{
    mEncOtherChk->setText(text);
}

void SignEncryptWidget::setEncryptWithPasswordText(const QString& text)
{
    mSymmetric->setText(text);
}

CertificateLineEdit *SignEncryptWidget::addRecipientWidget()
{
    auto certSel = new CertificateLineEdit(mModel,
                                           new EncryptCertificateFilter(mCurrentProto),
                                           this);
    certSel->setAccessibleName(i18nc("text for screen readers", "recipient key"));
    certSel->setEnabled(mEncOtherChk->isChecked());
    mRecpWidgets << certSel;

    if (mRecpLayout->count() > 0) {
        auto lastWidget = mRecpLayout->itemAt(mRecpLayout->count() - 1)->widget();
        setTabOrder(lastWidget, certSel);
    }
    mRecpLayout->addWidget(certSel);

    connect(certSel, &CertificateLineEdit::keyChanged,
            this, &SignEncryptWidget::recipientsChanged);
    connect(certSel, &CertificateLineEdit::wantsRemoval,
            this, &SignEncryptWidget::recpRemovalRequested);
    connect(certSel, &CertificateLineEdit::editingStarted,
            this, &SignEncryptWidget::recipientsChanged);
    connect(certSel, &CertificateLineEdit::dialogRequested,
            this, [this, certSel] () { dialogRequested(certSel); });
    connect(certSel, &CertificateLineEdit::certificateSelectionRequested,
            this, [this, certSel]() { certificateSelectionRequested(certSel); });

    return certSel;
}

void SignEncryptWidget::addRecipient(const Key &key)
{
    CertificateLineEdit *certSel = addRecipientWidget();
    if (!key.isNull()) {
        certSel->setKey(key);
        mAddedKeys << key;
    }
}

void SignEncryptWidget::addRecipient(const KeyGroup &group)
{
    CertificateLineEdit *certSel = addRecipientWidget();
    if (!group.isNull()) {
        certSel->setGroup(group);
        mAddedGroups << group;
    }
}

void SignEncryptWidget::dialogRequested(CertificateLineEdit *certificateLineEdit)
{
    if (!certificateLineEdit->key().isNull()) {
        auto cmd = new Commands::DetailsCommand(certificateLineEdit->key(), nullptr);
        cmd->start();
        return;
    }
    if (!certificateLineEdit->group().isNull()) {
        auto dlg = new GroupDetailsDialog;
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setGroup(certificateLineEdit->group());
        dlg->show();
        return;
    }

    certificateSelectionRequested(certificateLineEdit);
}

void SignEncryptWidget::certificateSelectionRequested(CertificateLineEdit *certificateLineEdit)
{
    CertificateSelectionDialog dlg{this};

    dlg.setOptions(CertificateSelectionDialog::Options(
        CertificateSelectionDialog::MultiSelection |
        CertificateSelectionDialog::EncryptOnly |
        CertificateSelectionDialog::optionsFromProtocol(mCurrentProto) |
        CertificateSelectionDialog::IncludeGroups));
    dlg.setStringFilter(certificateLineEdit->text());

    if (dlg.exec()) {
        const std::vector<Key> keys = dlg.selectedCertificates();
        const std::vector<KeyGroup> groups = dlg.selectedGroups();
        if (keys.size() == 0 && groups.size() == 0) {
            return;
        }
        bool isFirstItem = true;
        for (const Key &key : keys) {
            if (isFirstItem) {
                certificateLineEdit->setKey(key);
                isFirstItem = false;
            } else {
                addRecipient(key);
            }
        }
        for (const KeyGroup &group : groups) {
            if (isFirstItem) {
                certificateLineEdit->setGroup(group);
                isFirstItem = false;
            } else {
                addRecipient(group);
            }
        }
    }

    recipientsChanged();
}

void SignEncryptWidget::clearAddedRecipients()
{
    for (auto w: std::as_const(mUnknownWidgets)) {
        mRecpLayout->removeWidget(w);
        delete w;
    }

    for (auto &key: std::as_const(mAddedKeys)) {
        removeRecipient(key);
    }

    for (auto &group: std::as_const(mAddedGroups)) {
        removeRecipient(group);
    }
}

void SignEncryptWidget::addUnknownRecipient(const char *keyID)
{
    auto unknownWidget = new UnknownRecipientWidget(keyID);
    mUnknownWidgets << unknownWidget;

    if (mRecpLayout->count() > 0) {
        auto lastWidget = mRecpLayout->itemAt(mRecpLayout->count() - 1)->widget();
        setTabOrder(lastWidget, unknownWidget);
    }
    mRecpLayout->addWidget(unknownWidget);

    connect(KeyCache::instance().get(), &Kleo::KeyCache::keysMayHaveChanged,
            this, [this] () {
        // Check if any unknown recipient can now be found.
        for (auto w: mUnknownWidgets) {
            auto key = KeyCache::instance()->findByKeyIDOrFingerprint(w->keyID().toLatin1().constData());
            if (key.isNull()) {
                std::vector<std::string> subids;
                subids.push_back(std::string(w->keyID().toLatin1().constData()));
                for (const auto &subkey: KeyCache::instance()->findSubkeysByKeyID(subids)) {
                    key = subkey.parent();
                }
            }
            if (key.isNull()) {
                continue;
            }
            // Key is now available replace by line edit.
            qCDebug(KLEOPATRA_LOG) << "Removing widget for keyid: " << w->keyID();
            mRecpLayout->removeWidget(w);
            mUnknownWidgets.removeAll(w);
            delete w;
            addRecipient(key);
        }
    });
}

void SignEncryptWidget::recipientsChanged()
{
    const bool hasEmptyRecpWidget =
        std::any_of(std::cbegin(mRecpWidgets), std::cend(mRecpWidgets),
                    [](auto w) { return w->isEmpty(); });
    if (!hasEmptyRecpWidget) {
        addRecipientWidget();
    }
    updateOp();
}

Key SignEncryptWidget::signKey() const
{
    if (mSigSelect->isEnabled()) {
        return mSigSelect->currentKey();
    }
    return Key();
}

Key SignEncryptWidget::selfKey() const
{
    if (mSelfSelect->isEnabled()) {
        return mSelfSelect->currentKey();
    }
    return Key();
}

std::vector<Key> SignEncryptWidget::recipients() const
{
    std::vector<Key> ret;
    for (const CertificateLineEdit *w : std::as_const(mRecpWidgets)) {
        if (!w->isEnabled()) {
            // If one is disabled, all are disabled.
            break;
        }
        const Key k = w->key();
        const KeyGroup g = w->group();
        if (!k.isNull()) {
            ret.push_back(k);
        } else if (!g.isNull()) {
            const auto keys = g.keys();
            std::copy(keys.begin(), keys.end(), std::back_inserter(ret));
        }
    }
    const Key k = selfKey();
    if (!k.isNull()) {
        ret.push_back(k);
    }
    return ret;
}

bool SignEncryptWidget::isDeVsAndValid() const
{
    if (!signKey().isNull()
        && (!IS_DE_VS(signKey()) || keyValidity(signKey()) < GpgME::UserID::Validity::Full)) {
        return false;
    }

    if (!selfKey().isNull()
        && (!IS_DE_VS(selfKey()) || keyValidity(selfKey()) < GpgME::UserID::Validity::Full)) {
        return false;
    }

    for (const auto &key: recipients()) {
        if (!IS_DE_VS(key) || keyValidity(key) < GpgME::UserID::Validity::Full) {
            return false;
        }
    }

    return true;
}

void SignEncryptWidget::updateOp()
{
    const Key sigKey = signKey();
    const std::vector<Key> recp = recipients();

    QString newOp;
    if (!sigKey.isNull() && (!recp.empty() || encryptSymmetric())) {
        newOp = i18nc("@action", "Sign / Encrypt");
    } else if (!recp.empty() || encryptSymmetric()) {
        newOp = i18nc("@action", "Encrypt");
    } else if (!sigKey.isNull()) {
        newOp = i18nc("@action", "Sign");
    } else {
        newOp = QString();
    }
    mOp = newOp;
    Q_EMIT operationChanged(mOp);
    Q_EMIT keysChanged();
}

QString SignEncryptWidget::currentOp() const
{
    return mOp;
}

void SignEncryptWidget::recpRemovalRequested(CertificateLineEdit *w)
{
    if (!w) {
        return;
    }
    const int emptyEdits =
        std::count_if(std::cbegin(mRecpWidgets), std::cend(mRecpWidgets),
                      [](auto w) { return w->isEmpty(); });
    if (emptyEdits > 1) {
        if (w->hasFocus()) {
            const int index = mRecpLayout->indexOf(w);
            const auto focusWidget = (index < mRecpLayout->count() - 1) ?
                mRecpLayout->itemAt(index + 1)->widget() :
                mRecpLayout->itemAt(mRecpLayout->count() - 2)->widget();
            focusWidget->setFocus();
        }
        mRecpLayout->removeWidget(w);
        mRecpWidgets.removeAll(w);
        w->deleteLater();
    }
}

void SignEncryptWidget::removeRecipient(const GpgME::Key &key)
{
    for (CertificateLineEdit *edit: std::as_const(mRecpWidgets)) {
        const auto editKey = edit->key();
        if (key.isNull() && editKey.isNull()) {
            recpRemovalRequested(edit);
            return;
        }
        if (editKey.primaryFingerprint() &&
            key.primaryFingerprint() &&
            !strcmp(editKey.primaryFingerprint(), key.primaryFingerprint())) {
            recpRemovalRequested(edit);
            return;
        }
    }
}

void SignEncryptWidget::removeRecipient(const KeyGroup &group)
{
    for (CertificateLineEdit *edit: std::as_const(mRecpWidgets)) {
        const auto editGroup = edit->group();
        if (group.isNull() && editGroup.isNull()) {
            recpRemovalRequested(edit);
            return;
        }
        if (editGroup.name() == group.name()) {
            recpRemovalRequested(edit);
            return;
        }
    }
}

bool SignEncryptWidget::encryptSymmetric() const
{
    return mSymmetric->isChecked();
}

void SignEncryptWidget::loadKeys()
{
    KConfigGroup keys(KSharedConfig::openConfig(), "SignEncryptKeys");
    auto cache = KeyCache::instance();
    mSigSelect->setDefaultKey(keys.readEntry("SigningKey", QString()));
    mSelfSelect->setDefaultKey(keys.readEntry("EncryptKey", QString()));
}

void SignEncryptWidget::saveOwnKeys() const
{
    KConfigGroup keys(KSharedConfig::openConfig(), "SignEncryptKeys");
    auto sigKey = mSigSelect->currentKey();
    auto encKey = mSelfSelect->currentKey();
    if (!sigKey.isNull()) {
        keys.writeEntry("SigningKey", sigKey.primaryFingerprint());
    }
    if (!encKey.isNull()) {
        keys.writeEntry("EncryptKey", encKey.primaryFingerprint());
    }
}

void SignEncryptWidget::setSigningChecked(bool value)
{
    mSigChk->setChecked(value && !KeyCache::instance()->secretKeys().empty());
}

void SignEncryptWidget::setEncryptionChecked(bool checked)
{
    if (checked) {
        const bool haveSecretKeys = !KeyCache::instance()->secretKeys().empty();
        const bool havePublicKeys = !KeyCache::instance()->keys().empty();
        const bool symmetricOnly = FileOperationsPreferences().symmetricEncryptionOnly();
        mEncSelfChk->setChecked(haveSecretKeys && !symmetricOnly);
        mEncOtherChk->setChecked(havePublicKeys && !symmetricOnly);
        mSymmetric->setChecked(symmetricOnly || !havePublicKeys);
    } else {
        mEncSelfChk->setChecked(false);
        mEncOtherChk->setChecked(false);
        mSymmetric->setChecked(false);
    }
}

void SignEncryptWidget::setProtocol(GpgME::Protocol proto)
{
    if (mCurrentProto == proto) {
        return;
    }
    mCurrentProto = proto;
    onProtocolChanged();
}

void Kleo::SignEncryptWidget::onProtocolChanged()
{
    mSigSelect->setKeyFilter(std::shared_ptr<KeyFilter>(new SignCertificateFilter(mCurrentProto)));
    mSelfSelect->setKeyFilter(std::shared_ptr<KeyFilter>(new EncryptSelfCertificateFilter(mCurrentProto)));
    const auto encFilter = std::shared_ptr<KeyFilter>(new EncryptCertificateFilter(mCurrentProto));
    for (CertificateLineEdit *edit : std::as_const(mRecpWidgets)) {
        edit->setKeyFilter(encFilter);
    }

    if (mIsExclusive) {
        mSymmetric->setDisabled(mCurrentProto == GpgME::CMS);
        if (mSymmetric->isChecked() && mCurrentProto == GpgME::CMS) {
            mSymmetric->setChecked(false);
        }
        if (mSigChk->isChecked() && mCurrentProto == GpgME::CMS &&
                (mEncSelfChk->isChecked() || mEncOtherChk->isChecked())) {
            mSigChk->setChecked(false);
        }
    }
}

bool SignEncryptWidget::validate()
{
    QStringList unresolvedRecipients;
    for (const auto edit: std::as_const(mRecpWidgets)) {
        if (edit->isEnabled() && !edit->isEmpty() && edit->key().isNull() && edit->group().isNull()) {
            unresolvedRecipients.push_back(edit->text().toHtmlEscaped());
        }
    }
    if (!unresolvedRecipients.isEmpty()) {
        KMessageBox::errorList(this,
                               i18n("Could not find a key for the following recipients:"),
                               unresolvedRecipients,
                               i18n("Failed to find some keys"));
    }
    return unresolvedRecipients.isEmpty();
}

void SignEncryptWidget::updateCheckBoxes()
{
    const bool haveSecretKeys = !KeyCache::instance()->secretKeys().empty();
    const bool havePublicKeys = !KeyCache::instance()->keys().empty();
    const bool symmetricOnly = FileOperationsPreferences().symmetricEncryptionOnly();
    mSigChk->setEnabled(haveSecretKeys);
    mEncSelfChk->setEnabled(haveSecretKeys && !symmetricOnly);
    mEncOtherChk->setEnabled(havePublicKeys && !symmetricOnly);
    if (symmetricOnly) {
        mEncSelfChk->setChecked(false);
        mEncOtherChk->setChecked(false);
        mSymmetric->setChecked(true);
    }
}
