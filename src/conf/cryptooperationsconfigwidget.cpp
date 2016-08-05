/*
    cryptooperationsconfigwidget.cpp

    This file is part of kleopatra, the KDE key manager
    Copyright (c) 2010 Klarälvdalens Datakonsult AB

    Libkleopatra is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.

    Libkleopatra is distributed in the hope that it will be useful,
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

#include <config-kleopatra.h>

#include "cryptooperationsconfigwidget.h"

#include "emailoperationspreferences.h"
#include "fileoperationspreferences.h"

#include <Libkleo/ChecksumDefinition>

#include <gpgme++/context.h>

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>

#include <boost/shared_ptr.hpp>

using namespace Kleo;
using namespace Kleo::Config;
using namespace boost;

CryptoOperationsConfigWidget::CryptoOperationsConfigWidget(QWidget *p, Qt::WindowFlags f)
    : QWidget(p, f)
{
    setupGui();
}

void CryptoOperationsConfigWidget::setupGui()
{
    QVBoxLayout *baseLay = new QVBoxLayout;
    baseLay->setMargin(0);

    QGroupBox *mailGrp = new QGroupBox(i18n("EMail Operations"));
    QVBoxLayout *mailGrpLayout = new QVBoxLayout;
    mQuickSignCB = new QCheckBox(i18n("Don't confirm signing certificate if there is only one valid certificate for the identity"));
    mQuickEncryptCB = new QCheckBox(i18n("Don't confirm encryption certificates if there is exactly one valid certificate for each recipient"));
    mailGrpLayout->addWidget(mQuickSignCB);
    mailGrpLayout->addWidget(mQuickEncryptCB);
    mailGrp->setLayout(mailGrpLayout);
    baseLay->addWidget(mailGrp);

    QGroupBox *fileGrp = new QGroupBox(i18n("File Operations"));
    QVBoxLayout *fileGrpLay = new QVBoxLayout;
    mPGPFileExtCB = new QCheckBox(i18n("Create OpenPGP encrypted files with \".pgp\" file extensions instead of \".gpg\""));
    mAutoDecryptVerifyCB = new QCheckBox(i18n("Automatically start operation based on input detection for decrypt/verify."));
    fileGrpLay->addWidget(mPGPFileExtCB);
    fileGrpLay->addWidget(mAutoDecryptVerifyCB);
    QHBoxLayout *chkLay = new QHBoxLayout;
    QLabel *chkLabel = new QLabel(i18n("Checksum program to use when creating checksum files:"));
    chkLay->addWidget(chkLabel);
    mChecksumDefinitionCB = new QComboBox;
    chkLay->addWidget(mChecksumDefinitionCB);
    chkLay->addStretch(1);
    fileGrpLay->addLayout(chkLay);
    fileGrp->setLayout(fileGrpLay);
    baseLay->addWidget(fileGrp);

    baseLay->addStretch(1);

    setLayout(baseLay);

    if (!GpgME::hasFeature(0, GpgME::BinaryAndFineGrainedIdentify)) {
        /* Auto handling requires a working identify in GpgME.
         * so that classify in kleoaptra can correctly detect the input.*/
        mAutoDecryptVerifyCB->setVisible(false);
    }

    connect(mQuickSignCB, &QCheckBox::toggled, this, &CryptoOperationsConfigWidget::changed);
    connect(mQuickEncryptCB, &QCheckBox::toggled, this, &CryptoOperationsConfigWidget::changed);
    connect(mChecksumDefinitionCB, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &CryptoOperationsConfigWidget::changed);
    connect(mPGPFileExtCB, &QCheckBox::toggled, this, &CryptoOperationsConfigWidget::changed);
    connect(mAutoDecryptVerifyCB, &QCheckBox::toggled, this, &CryptoOperationsConfigWidget::changed);
}

CryptoOperationsConfigWidget::~CryptoOperationsConfigWidget() {}

void CryptoOperationsConfigWidget::defaults()
{
    EMailOperationsPreferences emailPrefs;
    emailPrefs.setDefaults();
    mQuickSignCB->setChecked(emailPrefs.quickSignEMail());
    mQuickEncryptCB->setChecked(emailPrefs.quickEncryptEMail());

    FileOperationsPreferences filePrefs;
    filePrefs.setDefaults();
    mPGPFileExtCB->setChecked(filePrefs.usePGPFileExt());
    mAutoDecryptVerifyCB->setChecked(filePrefs.autoDecryptVerify());

    if (mChecksumDefinitionCB->count()) {
        mChecksumDefinitionCB->setCurrentIndex(0);
    }
}

Q_DECLARE_METATYPE(boost::shared_ptr<Kleo::ChecksumDefinition>)

void CryptoOperationsConfigWidget::load()
{

    const EMailOperationsPreferences emailPrefs;
    mQuickSignCB   ->setChecked(emailPrefs.quickSignEMail());
    mQuickEncryptCB->setChecked(emailPrefs.quickEncryptEMail());

    const FileOperationsPreferences filePrefs;
    mPGPFileExtCB->setChecked(filePrefs.usePGPFileExt());
    mAutoDecryptVerifyCB->setChecked(filePrefs.autoDecryptVerify());

    const std::vector< shared_ptr<ChecksumDefinition> > cds = ChecksumDefinition::getChecksumDefinitions();
    const shared_ptr<ChecksumDefinition> default_cd = ChecksumDefinition::getDefaultChecksumDefinition(cds);

    mChecksumDefinitionCB->clear();

    Q_FOREACH (const shared_ptr<ChecksumDefinition> &cd, cds) {
        mChecksumDefinitionCB->addItem(cd->label(), qVariantFromValue(cd));
        if (cd == default_cd) {
            mChecksumDefinitionCB->setCurrentIndex(mChecksumDefinitionCB->count() - 1);
        }
    }
}

void CryptoOperationsConfigWidget::save()
{

    EMailOperationsPreferences emailPrefs;
    emailPrefs.setQuickSignEMail(mQuickSignCB   ->isChecked());
    emailPrefs.setQuickEncryptEMail(mQuickEncryptCB->isChecked());
    emailPrefs.save();

    FileOperationsPreferences filePrefs;
    filePrefs.setUsePGPFileExt(mPGPFileExtCB->isChecked());
    filePrefs.setAutoDecryptVerify(mAutoDecryptVerifyCB->isChecked());
    filePrefs.save();

    const int idx = mChecksumDefinitionCB->currentIndex();
    if (idx < 0) {
        return;    // ### pick first?
    }
    const shared_ptr<ChecksumDefinition> cd = qvariant_cast< shared_ptr<ChecksumDefinition> >(mChecksumDefinitionCB->itemData(idx));
    ChecksumDefinition::setDefaultChecksumDefinition(cd);

}

