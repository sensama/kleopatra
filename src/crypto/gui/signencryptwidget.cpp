/*  crypto/signencryptwidget.cpp

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

    QVBoxLayout *sigLay = new QVBoxLayout;
    QGroupBox *sigBox = new QGroupBox(i18nc("@label", "Protect Authenticity (Sign):"));
    sigBox->setCheckable(true);
    sigBox->setChecked(true);
    mSigSelect = new CertificateSelectionWidget(this,
                                                CertificateSelectionDialog::SignOnly |
                                                CertificateSelectionDialog::SecretKeys |
                                                CertificateSelectionDialog::AnyFormat,
                                                false);
    sigLay->addWidget(mSigSelect);
    sigBox->setLayout(sigLay);
    lay->addWidget(sigBox);

    connect(sigBox, &QGroupBox::toggled, mSigSelect, &QWidget::setEnabled);
    connect(sigBox, &QGroupBox::toggled, this, &SignEncryptWidget::updateOp);
    connect(mSigSelect, &CertificateSelectionWidget::keyChanged,
            this, &SignEncryptWidget::updateOp);

    setLayout(lay);
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
    return Key();
}

QVector <Key> SignEncryptWidget::recipients() const
{
    return QVector<Key>();
}

void SignEncryptWidget::updateOp()
{
    if (!signKey().isNull()) {
        mOp = i18nc("@action", "Sign");
    } else {
        mOp = QString();
    }
    Q_EMIT operationChanged(mOp);
}

QString SignEncryptWidget::currentOp() const
{
    return mOp;
}
