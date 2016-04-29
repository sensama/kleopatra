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

#include "certificateselectionwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QScrollArea>

#include <Libkleo/KeyRequester>

#include <KLocalizedString>
#include <KSharedConfig>

using namespace Kleo;
using namespace Kleo::Dialogs;
using namespace GpgME;

SignEncryptWidget::SignEncryptWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *lay = new QVBoxLayout;

    /* The signature selection */
    QHBoxLayout *sigLay = new QHBoxLayout;
    QCheckBox *sigChk = new QCheckBox(i18nc("@action", "Sign"));
    sigChk->setChecked(true);
    mSigSelect = new CertificateSelectionWidget(this,
                                                CertificateSelectionDialog::SignOnly |
                                                CertificateSelectionDialog::SecretKeys |
                                                CertificateSelectionDialog::AnyFormat,
                                                false);
    sigLay->addWidget(sigChk);
    sigLay->addWidget(mSigSelect);
    lay->addLayout(sigLay);

    connect(sigChk, &QCheckBox::toggled, mSigSelect, &QWidget::setEnabled);
    connect(sigChk, &QCheckBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(mSigSelect, &CertificateSelectionWidget::keyChanged,
            this, &SignEncryptWidget::updateOp);

    /* Recipient selection */
    mRecpLayout = new QVBoxLayout;
    mRecpLayout->setAlignment(Qt::AlignTop);
    mRecpLayout->addStretch(1);
    QGroupBox *encBox = new QGroupBox(i18nc("@action", "Encrypt"));
    encBox->setCheckable(true);
    encBox->setChecked(true);
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
    mSelfSelect = new CertificateSelectionWidget(this,
                                                CertificateSelectionDialog::EncryptOnly |
                                                CertificateSelectionDialog::SecretKeys |
                                                CertificateSelectionDialog::AnyFormat,
                                                false);
    encSelfLay->addWidget(encSelfChk);
    encSelfLay->addWidget(mSelfSelect);

    encBoxLay->addLayout(encSelfLay);

    connect(encSelfChk, &QCheckBox::toggled, mSelfSelect, &QWidget::setEnabled);
    connect(encSelfChk, &QCheckBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(mSelfSelect, &CertificateSelectionWidget::keyChanged,
            this, &SignEncryptWidget::updateOp);

    lay->addWidget(encBox);

    setLayout(lay);
    addRecipient();
    updateOp();
}

void SignEncryptWidget::addRecipient()
{
    CertificateSelectionWidget *certSel = new CertificateSelectionWidget(this,
                                                                         CertificateSelectionDialog::EncryptOnly |
                                                                         CertificateSelectionDialog::AnyFormat,
                                                                         true);
    mRecpWidgets << certSel;
    mRecpLayout->insertWidget(mRecpLayout->count() - 1, certSel);
    connect(certSel, &CertificateSelectionWidget::keyChanged,
            this, &SignEncryptWidget::recipientsChanged);
}

void SignEncryptWidget::recipientsChanged()
{
    bool oneEmpty = false;
    Q_FOREACH (const CertificateSelectionWidget *w, mRecpWidgets) {
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
    Q_FOREACH (const CertificateSelectionWidget *w, mRecpWidgets) {
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
