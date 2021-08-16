/*  crypto/gui/signencryptwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "signencryptwidget.h"

#include "kleopatra_debug.h"

#include "certificatelineedit.h"
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
      mRecpRowCount(2),
      mIsExclusive(sigEncExclusive)
{
    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    mModel->useKeyCache(true, KeyList::IncludeGroups);

    /* The signature selection */
    auto sigLay = new QHBoxLayout;
    auto sigGrp = new QGroupBox(i18n("Prove authenticity (sign)"));
    mSigChk = new QCheckBox(i18n("Sign as:"));
    mSigChk->setChecked(true);

    mSigSelect = new KeySelectionCombo();

    sigLay->addWidget(mSigChk);
    sigLay->addWidget(mSigSelect, 1);
    sigGrp->setLayout(sigLay);
    lay->addWidget(sigGrp);

    connect(mSigChk, &QCheckBox::toggled, mSigSelect, &QWidget::setEnabled);
    connect(mSigChk, &QCheckBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(mSigSelect, &KeySelectionCombo::currentKeyChanged,
            this, &SignEncryptWidget::updateOp);

    // Recipient selection
    mRecpLayout = new QGridLayout;
    mRecpLayout->setAlignment(Qt::AlignTop);
    auto encBoxLay = new QVBoxLayout;
    auto encBox = new QGroupBox(i18nc("@action", "Encrypt"));
    encBox->setLayout(encBoxLay);
    encBox->setAlignment(Qt::AlignLeft);

    // Own key
    mSelfSelect = new KeySelectionCombo();
    mEncSelfChk = new QCheckBox(i18n("Encrypt for me:"));
    mEncSelfChk->setChecked(true);
    mRecpLayout->addWidget(mEncSelfChk, 0, 0);
    mRecpLayout->addWidget(mSelfSelect, 0, 1);

    // Checkbox for other keys
    mEncOtherChk = new QCheckBox(i18n("Encrypt for others:"));
    mRecpLayout->addWidget(mEncOtherChk, 1, 0);
    mEncOtherChk->setChecked(true);
    connect(mEncOtherChk, &QCheckBox::toggled, this,
        [this](bool toggled) {
            for (CertificateLineEdit *edit : std::as_const(mRecpWidgets)) {
                edit->setEnabled(toggled);
            }
            updateOp();
        });

    // Scroll area for other keys
    auto recipientWidget = new QWidget;
    auto recipientScroll = new QScrollArea;
    recipientWidget->setLayout(mRecpLayout);
    recipientScroll->setWidget(recipientWidget);
    recipientScroll->setWidgetResizable(true);
    recipientScroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
    recipientScroll->setFrameStyle(QFrame::NoFrame);
    mRecpLayout->setContentsMargins(0, 0, 0, 0);
    encBoxLay->addWidget(recipientScroll, 1);

    auto bar = recipientScroll->verticalScrollBar();
    connect (bar, &QScrollBar::rangeChanged, this, [bar] (int, int max) {
            bar->setValue(max);
        });


    // Checkbox for password
    mSymmetric = new QCheckBox(i18n("Encrypt with password. Anyone you share the password with can read the data."));
    mSymmetric->setToolTip(i18nc("Tooltip information for symetric encryption",
                                 "Additionally to the keys of the recipients you can encrypt your data with a password. "
                                 "Anyone who has the password can read the data without any secret key. "
                                 "Using a password is <b>less secure</b> then public key cryptography. Even if you pick a very strong password."));
    encBoxLay->addWidget(mSymmetric);

    // Connect it
    connect(encBox, &QGroupBox::toggled, recipientWidget, &QWidget::setEnabled);
    connect(encBox, &QGroupBox::toggled, this, &SignEncryptWidget::updateOp);
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

    loadKeys();
    setProtocol(GpgME::UnknownProtocol);
    addRecipientWidget();
    updateOp();
}

CertificateLineEdit *SignEncryptWidget::addRecipientWidget()
{
    auto certSel = new CertificateLineEdit(mModel, this,
                                                           new EncryptCertificateFilter(mCurrentProto));
    mRecpWidgets << certSel;

    if (!mRecpLayout->itemAtPosition(mRecpRowCount - 1, 1)) {
        // First widget. Should align with the row above that
        // contains the encrypt for others checkbox.
        mRecpLayout->addWidget(certSel, mRecpRowCount - 1, 1);
    } else {
        mRecpLayout->addWidget(certSel, mRecpRowCount++, 1);
    }

    connect(certSel, &CertificateLineEdit::keyChanged,
            this, &SignEncryptWidget::recipientsChanged);
    connect(certSel, &CertificateLineEdit::wantsRemoval,
            this, &SignEncryptWidget::recpRemovalRequested);
    connect(certSel, &CertificateLineEdit::editingStarted,
            this, [this] () { addRecipientWidget(); });
    connect(certSel, &CertificateLineEdit::dialogRequested,
            this, [this, certSel] () { dialogRequested(certSel); });

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

    auto const dlg = new CertificateSelectionDialog(this);

    dlg->setOptions(CertificateSelectionDialog::Options(
        CertificateSelectionDialog::MultiSelection |
        CertificateSelectionDialog::EncryptOnly |
        CertificateSelectionDialog::optionsFromProtocol(mCurrentProto) |
        CertificateSelectionDialog::IncludeGroups));

    if (dlg->exec()) {
        const std::vector<Key> keys = dlg->selectedCertificates();
        const std::vector<KeyGroup> groups = dlg->selectedGroups();
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
    delete dlg;
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

    if (!mRecpLayout->itemAtPosition(mRecpRowCount - 1, 1)) {
        // First widget. Should align with the row above that
        // contains the encrypt for others checkbox.
        mRecpLayout->addWidget(unknownWidget, mRecpRowCount - 1, 1);
    } else {
        mRecpLayout->addWidget(unknownWidget, mRecpRowCount++, 1);
    }

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
    bool oneEmpty = false;
    for (const CertificateLineEdit *w : std::as_const(mRecpWidgets)) {
        if (w->key().isNull() && w->group().isNull()) {
            oneEmpty = true;
            break;
        }
    }
    if (!oneEmpty) {
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
    int emptyEdits = 0;
    for (const CertificateLineEdit *edit : std::as_const(mRecpWidgets)) {
        if (edit->isEmpty()) {
            emptyEdits++;
        }
        if (emptyEdits > 1) {
            int row, col, rspan, cspan;
            mRecpLayout->getItemPosition(mRecpLayout->indexOf(w), &row, &col, &rspan, &cspan);
            mRecpLayout->removeWidget(w);
            mRecpWidgets.removeAll(w);
            // The row count of the grid layout does not reflect the actual
            // items so we keep our internal count.
            mRecpRowCount--;
            for (int i = row + 1; i <= mRecpRowCount; i++) {
                // move widgets one up
                auto item = mRecpLayout->itemAtPosition(i, 1);
                if (!item) {
                    break;
                }
                mRecpLayout->removeItem(item);
                mRecpLayout->addItem(item, i - 1, 1);
            }
            w->deleteLater();
            return;
        }
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
    mSigChk->setChecked(value);
}

void SignEncryptWidget::setEncryptionChecked(bool checked)
{
    if (checked) {
        const bool haveOwnKeys = !KeyCache::instance()->secretKeys().empty();
        const bool haveOtherKeys = !KeyCache::instance()->keys().empty();
        const bool haveKeys = haveOwnKeys && haveOtherKeys;
        mEncSelfChk->setChecked(haveKeys);
        mEncOtherChk->setChecked(haveKeys);
        mSymmetric->setChecked(!haveKeys);
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
    mSigSelect->setKeyFilter(std::shared_ptr<KeyFilter>(new SignCertificateFilter(proto)));
    mSelfSelect->setKeyFilter(std::shared_ptr<KeyFilter>(new EncryptSelfCertificateFilter(proto)));
    const auto encFilter = std::shared_ptr<KeyFilter>(new EncryptCertificateFilter(proto));
    for (CertificateLineEdit *edit : std::as_const(mRecpWidgets)) {
        edit->setKeyFilter(encFilter);
    }

    if (mIsExclusive) {
        mSymmetric->setDisabled(proto == GpgME::CMS);
        if (mSymmetric->isChecked() && proto == GpgME::CMS) {
            mSymmetric->setChecked(false);
        }
        if (mSigChk->isChecked() && proto == GpgME::CMS &&
                (mEncSelfChk->isChecked() || mEncOtherChk->isChecked())) {
            mSigChk->setChecked(false);
        }
    }
}

bool SignEncryptWidget::validate()
{
    for (const auto edit: std::as_const(mRecpWidgets)) {
        if (!edit->isEmpty() && edit->key().isNull() && edit->group().isNull()) {
            KMessageBox::error(this, i18nc("%1 is user input that could not be found",
                        "Could not find a key for '%1'", edit->text().toHtmlEscaped()),
                    i18n("Failed to find recipient"), KMessageBox::Notify);
            return false;
        }
    }
    return true;
}
