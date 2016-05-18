/*  crypto/gui/signencryptwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2016 Intevation GmbH

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

#include "certificatecombobox.h"
#include "certificatelineedit.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QScrollArea>

#include <Libkleo/DefaultKeyFilter>

#include <KLocalizedString>
#include <KSharedConfig>

#include "models/keylistmodel.h"
#include "models/keylistsortfilterproxymodel.h"

using namespace Kleo;
using namespace Kleo::Dialogs;
using namespace GpgME;

namespace {
class SignCertificateFilter: public DefaultKeyFilter
{
public:
    SignCertificateFilter() : DefaultKeyFilter()
    {
        setRevoked(DefaultKeyFilter::NotSet);
        setExpired(DefaultKeyFilter::NotSet);
        setHasSecret(DefaultKeyFilter::Set);
        setCanSign(DefaultKeyFilter::Set);
    }
};
class EncryptCertificateFilter: public DefaultKeyFilter
{
public:
    EncryptCertificateFilter(): DefaultKeyFilter()
    {
        setRevoked(DefaultKeyFilter::NotSet);
        setExpired(DefaultKeyFilter::NotSet);
        setCanEncrypt(DefaultKeyFilter::Set);
    }
};
class EncryptSelfCertificateFilter: public EncryptCertificateFilter
{
public:
    EncryptSelfCertificateFilter(): EncryptCertificateFilter()
    {
        setRevoked(DefaultKeyFilter::NotSet);
        setExpired(DefaultKeyFilter::NotSet);
        setCanEncrypt(DefaultKeyFilter::Set);
        setHasSecret(DefaultKeyFilter::Set);
    }
};
}

SignEncryptWidget::SignEncryptWidget(QWidget *parent)
    : QWidget(parent),
      mModel(AbstractKeyListModel::createFlatKeyListModel(this))
{
    QVBoxLayout *lay = new QVBoxLayout;
    setContentsMargins(-6,-6,-6,-6);

    mModel->useKeyCache(true, false);

    /* The signature selection */
    QHBoxLayout *sigLay = new QHBoxLayout;
    QGroupBox *sigGrp = new QGroupBox(i18nc("@label", "Prove authenticity (sign)"));
    QCheckBox *sigChk = new QCheckBox(i18nc("@label", "Sign as:"));
    sigChk->setChecked(true);

    KeyListSortFilterProxyModel *sigModel = new KeyListSortFilterProxyModel(this);
    sigModel->setKeyFilter(boost::shared_ptr<KeyFilter>(new SignCertificateFilter()));
    sigModel->setSourceModel(mModel);
    mModel->setParent(this);

    mSigSelect = new CertificateComboBox(i18n("No valid secret keys found."));
    mSigSelect->setModel(sigModel);
    mSigSelect->setModelColumn(KeyListModelInterface::Summary);

    sigLay->addWidget(sigChk);
    sigLay->addStretch(1);
    sigLay->addWidget(mSigSelect);
    sigGrp->setLayout(sigLay);
    lay->addWidget(sigGrp);

    connect(sigChk, &QCheckBox::toggled, mSigSelect, &QWidget::setEnabled);
    connect(sigChk, &QCheckBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(mSigSelect, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &SignEncryptWidget::updateOp);

    /* Recipient selection */
    mRecpLayout = new QGridLayout;
    mRecpLayout->setAlignment(Qt::AlignTop);
    QGroupBox *encBox = new QGroupBox(i18nc("@action", "Encrypt"));
    encBox->setAlignment(Qt::AlignLeft);
    QVBoxLayout *encBoxLay = new QVBoxLayout;
    encBox->setLayout(encBoxLay);
    QWidget *recipientWidget = new QWidget;
    QScrollArea *recipientScroll = new QScrollArea;
    recipientWidget->setLayout(mRecpLayout);
    recipientScroll->setWidget(recipientWidget);
    recipientScroll->setWidgetResizable(true);
    recipientScroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
    encBoxLay->addWidget(recipientScroll, 1);
    connect(encBox, &QGroupBox::toggled, recipientWidget, &QWidget::setEnabled);
    connect(encBox, &QGroupBox::toggled, this, &SignEncryptWidget::updateOp);

    /* Self certificate */
    QHBoxLayout *encSelfLay = new QHBoxLayout;
    QCheckBox *encSelfChk = new QCheckBox(i18nc("@label", "Own certificate:"));
    encSelfChk->setChecked(true);

    KeyListSortFilterProxyModel *encModel = new KeyListSortFilterProxyModel(this);
    encModel->setKeyFilter(boost::shared_ptr<KeyFilter>(new EncryptSelfCertificateFilter()));
    encModel->setSourceModel(mModel);
    mModel->setParent(this);

    mSelfSelect = new CertificateComboBox(i18n("No valid secret keys found."));
    mSelfSelect->setModel(encModel);
    mSelfSelect->setModelColumn(KeyListModelInterface::Summary);
    encSelfLay->addWidget(encSelfChk);
    encSelfLay->addWidget(mSelfSelect);

    encBoxLay->addLayout(encSelfLay);

    connect(encSelfChk, &QCheckBox::toggled, mSelfSelect, &QWidget::setEnabled);
    connect(encSelfChk, &QCheckBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(mSelfSelect, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &SignEncryptWidget::updateOp);

    lay->addWidget(encBox);

    setLayout(lay);
    addRecipient();
    updateOp();
}

void SignEncryptWidget::addRecipient()
{
    addRecipient(Key());
}

void SignEncryptWidget::addRecipient(const Key &key)
{
    CertificateLineEdit *certSel = new CertificateLineEdit(mModel, this,
                                                           new EncryptCertificateFilter());
    mRecpWidgets << certSel;

    mRecpLayout->addWidget(certSel, mRecpLayout->rowCount(), 0);
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
    }
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
        return mSigSelect->key();
    }
    return Key();
}

Key SignEncryptWidget::selfKey() const
{
    if (mSelfSelect->isEnabled()) {
        return mSelfSelect->key();
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

void SignEncryptWidget::updateOp()
{
    const Key sigKey = signKey();
    const QVector<Key> recp = recipients();

    QString newOp;
    if (!sigKey.isNull() && !recp.isEmpty()) {
        newOp = i18nc("@action", "Sign / Encrypt");
    } else if (!recp.isEmpty()) {
        newOp = i18nc("@action", "Encrypt");
    } else if (!sigKey.isNull()) {
        newOp = i18nc("@action", "Sign");
    } else {
        newOp = QString();
    }
    if (newOp != mOp) {
        mOp = newOp;
        Q_EMIT operationChanged(mOp);
    }
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
            mRecpLayout->removeWidget(w);
            mRecpWidgets.removeAll(w);
            return;
        }
    }
}
