/*  crypto/gui/signencryptwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2016 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include "signencryptwidget.h"

#include "kleopatra_debug.h"

#include "certificatelineedit.h"
#include "unknownrecipientwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QScrollBar>

#include <Libkleo/DefaultKeyFilter>
#include <Libkleo/KeyCache>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeySelectionCombo>
#include <Libkleo/KeyListSortFilterProxyModel>

#include <utils/gnupg-helper.h>

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
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    mModel->useKeyCache(true, false);

    /* The signature selection */
    QHBoxLayout *sigLay = new QHBoxLayout;
    QGroupBox *sigGrp = new QGroupBox(i18n("Prove authenticity (sign)"));
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
    QVBoxLayout *encBoxLay = new QVBoxLayout;
    QGroupBox *encBox = new QGroupBox(i18nc("@action", "Encrypt"));
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
            Q_FOREACH (CertificateLineEdit *edit, mRecpWidgets) {
                edit->setEnabled(toggled);
            }
            updateOp();
        });

    // Scroll area for other keys
    QWidget *recipientWidget = new QWidget;
    QScrollArea *recipientScroll = new QScrollArea;
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
    addRecipient(Key());
    updateOp();
}

void SignEncryptWidget::addRecipient()
{
    addRecipient(Key());
}

void SignEncryptWidget::addRecipient(const Key &key)
{
    CertificateLineEdit *certSel = new CertificateLineEdit(mModel, this,
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
            this, static_cast<void (SignEncryptWidget::*)()>(&SignEncryptWidget::addRecipient));
    connect(certSel, &CertificateLineEdit::addRequested,
            this, static_cast<void (SignEncryptWidget::*)(const Key&)>(&SignEncryptWidget::addRecipient));

    if (!key.isNull()) {
        certSel->setKey(key);
        mAddedKeys << key;
    }
}

void SignEncryptWidget::clearAddedRecipients()
{
    for (auto w: qAsConst(mUnknownWidgets)) {
        mRecpLayout->removeWidget(w);
        delete w;
    }

    for (auto &key: qAsConst(mAddedKeys)) {
        removeRecipient(key);
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
    Q_FOREACH (const CertificateLineEdit *w, mRecpWidgets) {
        if (w->key().isNull()) {
            oneEmpty = true;
            break;
        }
    }
    if (!oneEmpty) {
        addRecipient();
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

QVector <Key> SignEncryptWidget::recipients() const
{
    QVector<Key> ret;
    Q_FOREACH (const CertificateLineEdit *w, mRecpWidgets) {
        if (!w->isEnabled()) {
            // If one is disabled, all are disabled.
            break;
        }
        const Key k = w->key();
        if (!k.isNull()) {
            ret << k;
        }
    }
    const Key k = selfKey();
    if (!k.isNull()) {
        ret << k;
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
    const QVector<Key> recp = recipients();

    QString newOp;
    if (!sigKey.isNull() && (!recp.isEmpty() || encryptSymmetric())) {
        newOp = i18nc("@action", "Sign / Encrypt");
    } else if (!recp.isEmpty() || encryptSymmetric()) {
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
    Q_FOREACH (const CertificateLineEdit *edit, mRecpWidgets) {
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
    for (CertificateLineEdit *edit: qAsConst(mRecpWidgets)) {
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

void SignEncryptWidget::setEncryptionChecked(bool value)
{
    mEncSelfChk->setChecked(value);
    mEncOtherChk->setChecked(value);
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
    Q_FOREACH (CertificateLineEdit *edit, mRecpWidgets) {
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
    for (const auto edit: qAsConst(mRecpWidgets)) {
        if (!edit->isEmpty() && edit->key().isNull()) {
            KMessageBox::error(this, i18nc("%1 is user input that could not be found",
                        "Could not find a key for '%1'", edit->text().toHtmlEscaped()),
                    i18n("Failed to find recipient"), KMessageBox::Notify);
            return false;
        }
    }
    return true;
}
