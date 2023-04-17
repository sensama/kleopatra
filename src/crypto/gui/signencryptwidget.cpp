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

#include "dialogs/certificateselectiondialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QScrollBar>

#include <Libkleo/Compliance>
#include <Libkleo/DefaultKeyFilter>
#include <Libkleo/KeyCache>
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
        setValidIfSMIME(DefaultKeyFilter::Set);

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
        setValidIfSMIME(DefaultKeyFilter::Set);

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
        setValidIfSMIME(DefaultKeyFilter::Set);
    }
};
}

class SignEncryptWidget::Private
{
    SignEncryptWidget *const q;

public:
    explicit Private(SignEncryptWidget *qq, bool sigEncExclusive)
        : q{qq}
        , mModel{AbstractKeyListModel::createFlatKeyListModel(qq)}
        , mIsExclusive{sigEncExclusive}
    {
    }

    CertificateLineEdit* addRecipientWidget();
    /* Inserts a new recipient widget after widget @p after or at the end
     * if @p after is null. */
    CertificateLineEdit* insertRecipientWidget(CertificateLineEdit *after);
    void onProtocolChanged();
    void updateCheckBoxes();

public:
    KeySelectionCombo *mSigSelect = nullptr;
    KeySelectionCombo *mSelfSelect = nullptr;
    QVector<CertificateLineEdit *> mRecpWidgets;
    QVector<UnknownRecipientWidget *> mUnknownWidgets;
    QVector<GpgME::Key> mAddedKeys;
    QVector<KeyGroup> mAddedGroups;
    QVBoxLayout *mRecpLayout = nullptr;
    Operations mOp;
    AbstractKeyListModel *mModel = nullptr;
    QCheckBox *mSymmetric = nullptr;
    QCheckBox *mSigChk = nullptr;
    QCheckBox *mEncOtherChk = nullptr;
    QCheckBox *mEncSelfChk = nullptr;
    GpgME::Protocol mCurrentProto = GpgME::UnknownProtocol;
    const bool mIsExclusive;
};

SignEncryptWidget::SignEncryptWidget(QWidget *parent, bool sigEncExclusive)
    : QWidget{parent}
    , d{new Private{this, sigEncExclusive}}
{
    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    d->mModel->useKeyCache(true, KeyList::IncludeGroups);

    const bool haveSecretKeys = !KeyCache::instance()->secretKeys().empty();
    const bool havePublicKeys = !KeyCache::instance()->keys().empty();
    const bool symmetricOnly = FileOperationsPreferences().symmetricEncryptionOnly();

    /* The signature selection */
    auto sigLay = new QHBoxLayout;
    auto sigGrp = new QGroupBox(i18nc("@title:group", "Prove authenticity (sign)"));
    d->mSigChk = new QCheckBox(i18n("Sign as:"));
    d->mSigChk->setEnabled(haveSecretKeys);
    d->mSigChk->setChecked(haveSecretKeys);

    d->mSigSelect = new KeySelectionCombo();
    d->mSigSelect->setEnabled(d->mSigChk->isChecked());

    sigLay->addWidget(d->mSigChk);
    sigLay->addWidget(d->mSigSelect, 1);
    sigGrp->setLayout(sigLay);
    lay->addWidget(sigGrp);

    connect(d->mSigChk, &QCheckBox::toggled, d->mSigSelect, &QWidget::setEnabled);
    connect(d->mSigChk, &QCheckBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(d->mSigSelect, &KeySelectionCombo::currentKeyChanged,
            this, &SignEncryptWidget::updateOp);

    // Recipient selection
    auto encBoxLay = new QVBoxLayout;
    auto encBox = new QGroupBox(i18nc("@title:group", "Encrypt"));
    encBox->setLayout(encBoxLay);
    auto recipientGrid = new QGridLayout;

    // Own key
    d->mEncSelfChk = new QCheckBox(i18n("Encrypt for me:"));
    d->mEncSelfChk->setEnabled(haveSecretKeys && !symmetricOnly);
    d->mEncSelfChk->setChecked(haveSecretKeys && !symmetricOnly);
    d->mSelfSelect = new KeySelectionCombo();
    d->mSelfSelect->setEnabled(d->mEncSelfChk->isChecked());
    recipientGrid->addWidget(d->mEncSelfChk, 0, 0);
    recipientGrid->addWidget(d->mSelfSelect, 0, 1);

    // Checkbox for other keys
    d->mEncOtherChk = new QCheckBox(i18n("Encrypt for others:"));
    d->mEncOtherChk->setEnabled(havePublicKeys && !symmetricOnly);
    d->mEncOtherChk->setChecked(havePublicKeys && !symmetricOnly);
    recipientGrid->addWidget(d->mEncOtherChk, 1, 0, Qt::AlignTop);
    connect(d->mEncOtherChk, &QCheckBox::toggled, this,
        [this](bool toggled) {
            for (CertificateLineEdit *edit : std::as_const(d->mRecpWidgets)) {
                edit->setEnabled(toggled);
            }
            updateOp();
        });
    d->mRecpLayout = new QVBoxLayout;
    recipientGrid->addLayout(d->mRecpLayout, 1, 1);
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

    d->addRecipientWidget();

    // Checkbox for password
    d->mSymmetric = new QCheckBox(i18n("Encrypt with password. Anyone you share the password with can read the data."));
    d->mSymmetric->setToolTip(i18nc("Tooltip information for symmetric encryption",
                                 "Additionally to the keys of the recipients you can encrypt your data with a password. "
                                 "Anyone who has the password can read the data without any secret key. "
                                 "Using a password is <b>less secure</b> then public key cryptography. Even if you pick a very strong password."));
    d->mSymmetric->setChecked(symmetricOnly || !havePublicKeys);
    encBoxLay->addWidget(d->mSymmetric);

    // Connect it
    connect(d->mEncSelfChk, &QCheckBox::toggled, d->mSelfSelect, &QWidget::setEnabled);
    connect(d->mEncSelfChk, &QCheckBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(d->mSymmetric, &QCheckBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(d->mSelfSelect, &KeySelectionCombo::currentKeyChanged,
            this, &SignEncryptWidget::updateOp);

    if (d->mIsExclusive) {
        connect(d->mEncOtherChk, &QCheckBox::toggled, this, [this](bool value) {
            if (d->mCurrentProto != GpgME::CMS) {
                return;
            }
            if (value) {
                d->mSigChk->setChecked(false);
            }
        });
        connect(d->mEncSelfChk, &QCheckBox::toggled, this, [this](bool value) {
            if (d->mCurrentProto != GpgME::CMS) {
                return;
            }
            if (value) {
                d->mSigChk->setChecked(false);
            }
        });
        connect(d->mSigChk, &QCheckBox::toggled, this, [this](bool value) {
            if (d->mCurrentProto != GpgME::CMS) {
                return;
            }
            if (value) {
                d->mEncSelfChk->setChecked(false);
                d->mEncOtherChk->setChecked(false);
            }
        });
    }

    // Ensure that the d->mSigChk is aligned together with the encryption check boxes.
    d->mSigChk->setMinimumWidth(qMax(d->mEncOtherChk->width(), d->mEncSelfChk->width()));

    lay->addWidget(encBox);

    connect(KeyCache::instance().get(), &Kleo::KeyCache::keysMayHaveChanged,
            this, [this]() { d->updateCheckBoxes(); });
    connect(KleopatraApplication::instance(), &KleopatraApplication::configurationChanged,
            this, [this]() { d->updateCheckBoxes(); });

    loadKeys();
    d->onProtocolChanged();
    updateOp();
}

SignEncryptWidget::~SignEncryptWidget() = default;

void SignEncryptWidget::setSignAsText(const QString &text)
{
    d->mSigChk->setText(text);
}

void SignEncryptWidget::setEncryptForMeText(const QString &text)
{
    d->mEncSelfChk->setText(text);
}

void SignEncryptWidget::setEncryptForOthersText(const QString &text)
{
    d->mEncOtherChk->setText(text);
}

void SignEncryptWidget::setEncryptWithPasswordText(const QString& text)
{
    d->mSymmetric->setText(text);
}

CertificateLineEdit *SignEncryptWidget::Private::addRecipientWidget()
{
    return insertRecipientWidget(nullptr);
}

CertificateLineEdit *SignEncryptWidget::Private::insertRecipientWidget(CertificateLineEdit *after)
{
    Q_ASSERT(!after || mRecpLayout->indexOf(after) != -1);

    const auto index = after ? mRecpLayout->indexOf(after) + 1 : mRecpLayout->count();

    auto certSel = new CertificateLineEdit(mModel,
                                           new EncryptCertificateFilter(mCurrentProto),
                                           q);
    certSel->setAccessibleNameOfLineEdit(i18nc("text for screen readers", "recipient key"));
    certSel->setEnabled(mEncOtherChk->isChecked());
    mRecpWidgets.insert(index, certSel);

    if (mRecpLayout->count() > 0) {
        auto prevWidget = after ? after : mRecpLayout->itemAt(mRecpLayout->count() - 1)->widget();
        setTabOrder(prevWidget, certSel);
    }
    mRecpLayout->insertWidget(index, certSel);

    connect(certSel, &CertificateLineEdit::keyChanged,
            q, &SignEncryptWidget::recipientsChanged);
    connect(certSel, &CertificateLineEdit::editingStarted,
            q, &SignEncryptWidget::recipientsChanged);
    connect(certSel, &CertificateLineEdit::certificateSelectionRequested,
            q, [this, certSel]() { q->certificateSelectionRequested(certSel); });

    return certSel;
}

void SignEncryptWidget::addRecipient(const Key &key)
{
    CertificateLineEdit *certSel = d->addRecipientWidget();
    if (!key.isNull()) {
        certSel->setKey(key);
        d->mAddedKeys << key;
    }
}

void SignEncryptWidget::addRecipient(const KeyGroup &group)
{
    CertificateLineEdit *certSel = d->addRecipientWidget();
    if (!group.isNull()) {
        certSel->setGroup(group);
        d->mAddedGroups << group;
    }
}

void SignEncryptWidget::certificateSelectionRequested(CertificateLineEdit *certificateLineEdit)
{
    CertificateSelectionDialog dlg{this};

    dlg.setOptions(CertificateSelectionDialog::Options(
        CertificateSelectionDialog::MultiSelection |
        CertificateSelectionDialog::EncryptOnly |
        CertificateSelectionDialog::optionsFromProtocol(d->mCurrentProto) |
        CertificateSelectionDialog::IncludeGroups));

    if (!certificateLineEdit->key().isNull()) {
        const auto key = certificateLineEdit->key();
        const auto name = QString::fromUtf8(key.userID(0).name());
        const auto email = QString::fromUtf8(key.userID(0).email());
        dlg.setStringFilter(!name.isEmpty() ? name : email);
    } else if (!certificateLineEdit->group().isNull()) {
        dlg.setStringFilter(certificateLineEdit->group().name());
    } else {
        dlg.setStringFilter(certificateLineEdit->text());
    }

    if (dlg.exec()) {
        const std::vector<Key> keys = dlg.selectedCertificates();
        const std::vector<KeyGroup> groups = dlg.selectedGroups();
        if (keys.size() == 0 && groups.size() == 0) {
            return;
        }
        CertificateLineEdit *certWidget = nullptr;
        for (const Key &key : keys) {
            if (!certWidget) {
                certWidget = certificateLineEdit;
            } else {
                certWidget = d->insertRecipientWidget(certWidget);
            }
            certWidget->setKey(key);
        }
        for (const KeyGroup &group : groups) {
            if (!certWidget) {
                certWidget = certificateLineEdit;
            } else {
                certWidget = d->insertRecipientWidget(certWidget);
            }
            certWidget->setGroup(group);
        }
    }

    recipientsChanged();
}

void SignEncryptWidget::clearAddedRecipients()
{
    for (auto w: std::as_const(d->mUnknownWidgets)) {
        d->mRecpLayout->removeWidget(w);
        delete w;
    }

    for (auto &key: std::as_const(d->mAddedKeys)) {
        removeRecipient(key);
    }

    for (auto &group: std::as_const(d->mAddedGroups)) {
        removeRecipient(group);
    }
}

void SignEncryptWidget::addUnknownRecipient(const char *keyID)
{
    auto unknownWidget = new UnknownRecipientWidget(keyID);
    d->mUnknownWidgets << unknownWidget;

    if (d->mRecpLayout->count() > 0) {
        auto lastWidget = d->mRecpLayout->itemAt(d->mRecpLayout->count() - 1)->widget();
        setTabOrder(lastWidget, unknownWidget);
    }
    d->mRecpLayout->addWidget(unknownWidget);

    connect(KeyCache::instance().get(), &Kleo::KeyCache::keysMayHaveChanged,
            this, [this] () {
        // Check if any unknown recipient can now be found.
        for (auto w: d->mUnknownWidgets) {
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
            d->mRecpLayout->removeWidget(w);
            d->mUnknownWidgets.removeAll(w);
            delete w;
            addRecipient(key);
        }
    });
}

void SignEncryptWidget::recipientsChanged()
{
    const bool hasEmptyRecpWidget =
        std::any_of(std::cbegin(d->mRecpWidgets), std::cend(d->mRecpWidgets),
                    [](auto w) { return w->isEmpty(); });
    if (!hasEmptyRecpWidget) {
        d->addRecipientWidget();
    }
    updateOp();
}

Key SignEncryptWidget::signKey() const
{
    if (d->mSigSelect->isEnabled()) {
        return d->mSigSelect->currentKey();
    }
    return Key();
}

Key SignEncryptWidget::selfKey() const
{
    if (d->mSelfSelect->isEnabled()) {
        return d->mSelfSelect->currentKey();
    }
    return Key();
}

std::vector<Key> SignEncryptWidget::recipients() const
{
    std::vector<Key> ret;
    for (const CertificateLineEdit *w : std::as_const(d->mRecpWidgets)) {
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
    if (!signKey().isNull() && !DeVSCompliance::keyIsCompliant(signKey())) {
        return false;
    }

    if (!selfKey().isNull() && !DeVSCompliance::keyIsCompliant(selfKey())) {
        return false;
    }

    for (const auto &key: recipients()) {
        if (!DeVSCompliance::keyIsCompliant(key)) {
            return false;
        }
    }

    return true;
}

void SignEncryptWidget::updateOp()
{
    const Key sigKey = signKey();
    const std::vector<Key> recp = recipients();

    Operations op = NoOperation;
    if (!sigKey.isNull()) {
        op |= Sign;
    }
    if (!recp.empty() || encryptSymmetric()) {
        op |= Encrypt;
    }
    d->mOp = op;
    Q_EMIT operationChanged(d->mOp);
    Q_EMIT keysChanged();
}

SignEncryptWidget::Operations SignEncryptWidget::currentOp() const
{
    return d->mOp;
}

namespace
{
bool recipientWidgetHasFocus(CertificateLineEdit *w)
{
    // check if w (or its focus proxy) or a child widget of w has focus
    return w->hasFocus() || w->isAncestorOf(qApp->focusWidget());
}
}

void SignEncryptWidget::recpRemovalRequested(CertificateLineEdit *w)
{
    if (!w) {
        return;
    }
    const int emptyEdits =
        std::count_if(std::cbegin(d->mRecpWidgets), std::cend(d->mRecpWidgets),
                      [](auto w) { return w->isEmpty(); });
    if (emptyEdits > 1) {
        if (recipientWidgetHasFocus(w)) {
            const int index = d->mRecpLayout->indexOf(w);
            const auto focusWidget = (index < d->mRecpLayout->count() - 1) ?
                d->mRecpLayout->itemAt(index + 1)->widget() :
                d->mRecpLayout->itemAt(d->mRecpLayout->count() - 2)->widget();
            focusWidget->setFocus();
        }
        d->mRecpLayout->removeWidget(w);
        d->mRecpWidgets.removeAll(w);
        w->deleteLater();
    }
}

void SignEncryptWidget::removeRecipient(const GpgME::Key &key)
{
    for (CertificateLineEdit *edit: std::as_const(d->mRecpWidgets)) {
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
    for (CertificateLineEdit *edit: std::as_const(d->mRecpWidgets)) {
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
    return d->mSymmetric->isChecked();
}

void SignEncryptWidget::loadKeys()
{
    KConfigGroup keys(KSharedConfig::openConfig(), "SignEncryptKeys");
    auto cache = KeyCache::instance();
    d->mSigSelect->setDefaultKey(keys.readEntry("SigningKey", QString()));
    d->mSelfSelect->setDefaultKey(keys.readEntry("EncryptKey", QString()));
}

void SignEncryptWidget::saveOwnKeys() const
{
    KConfigGroup keys(KSharedConfig::openConfig(), "SignEncryptKeys");
    auto sigKey = d->mSigSelect->currentKey();
    auto encKey = d->mSelfSelect->currentKey();
    if (!sigKey.isNull()) {
        keys.writeEntry("SigningKey", sigKey.primaryFingerprint());
    }
    if (!encKey.isNull()) {
        keys.writeEntry("EncryptKey", encKey.primaryFingerprint());
    }
}

void SignEncryptWidget::setSigningChecked(bool value)
{
    d->mSigChk->setChecked(value && !KeyCache::instance()->secretKeys().empty());
}

void SignEncryptWidget::setEncryptionChecked(bool checked)
{
    if (checked) {
        const bool haveSecretKeys = !KeyCache::instance()->secretKeys().empty();
        const bool havePublicKeys = !KeyCache::instance()->keys().empty();
        const bool symmetricOnly = FileOperationsPreferences().symmetricEncryptionOnly();
        d->mEncSelfChk->setChecked(haveSecretKeys && !symmetricOnly);
        d->mEncOtherChk->setChecked(havePublicKeys && !symmetricOnly);
        d->mSymmetric->setChecked(symmetricOnly || !havePublicKeys);
    } else {
        d->mEncSelfChk->setChecked(false);
        d->mEncOtherChk->setChecked(false);
        d->mSymmetric->setChecked(false);
    }
}

void SignEncryptWidget::setProtocol(GpgME::Protocol proto)
{
    if (d->mCurrentProto == proto) {
        return;
    }
    d->mCurrentProto = proto;
    d->onProtocolChanged();
}

void Kleo::SignEncryptWidget::Private::onProtocolChanged()
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

bool SignEncryptWidget::isComplete() const
{
    return currentOp() != NoOperation
        && std::all_of(std::cbegin(d->mRecpWidgets), std::cend(d->mRecpWidgets),
                        [](auto w) { return !w->isEnabled() || w->hasAcceptableInput(); });
}

bool SignEncryptWidget::validate()
{
    CertificateLineEdit *firstUnresolvedRecipient = nullptr;
    QStringList unresolvedRecipients;
    for (const auto edit: std::as_const(d->mRecpWidgets)) {
        if (edit->isEnabled() && !edit->hasAcceptableInput()) {
            if (!firstUnresolvedRecipient) {
                firstUnresolvedRecipient = edit;
            }
            unresolvedRecipients.push_back(edit->text().toHtmlEscaped());
        }
    }
    if (!unresolvedRecipients.isEmpty()) {
        KMessageBox::errorList(this,
                               i18n("Could not find a key for the following recipients:"),
                               unresolvedRecipients,
                               i18n("Failed to find some keys"));
    }
    if (firstUnresolvedRecipient) {
        firstUnresolvedRecipient->setFocus();
    }
    return unresolvedRecipients.isEmpty();
}

void SignEncryptWidget::Private::updateCheckBoxes()
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
