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

#include <QGpgME/Protocol>

#include <gpgme++/context.h>

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QCheckBox>
#include <QComboBox>
#include <QLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QRegularExpression>

#include <memory>

using namespace Kleo;
using namespace Kleo::Config;

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
    mASCIIArmorCB = new QCheckBox(i18n("Create signed or encrypted files as text files."));
    mASCIIArmorCB->setToolTip(i18nc("@info", "Set this option to encode encrypted or signed files as base64 encoded text. "
                                             "So that they can be opened with an editor or sent in a mail body. "
                                             "This will increase file size by one third."));
    mAutoDecryptVerifyCB = new QCheckBox(i18n("Automatically start operation based on input detection for decrypt/verify."));
    fileGrpLay->addWidget(mPGPFileExtCB);
    fileGrpLay->addWidget(mAutoDecryptVerifyCB);
    fileGrpLay->addWidget(mASCIIArmorCB);

    QGridLayout *comboLay = new QGridLayout;
    QLabel *chkLabel = new QLabel(i18n("Checksum program to use when creating checksum files:"));
    comboLay->addWidget(chkLabel, 0, 0);
    mChecksumDefinitionCB = new QComboBox;
    comboLay->addWidget(mChecksumDefinitionCB, 0, 1);

    QLabel *archLabel = new QLabel(i18n("Archive command to use when archiving files:"));
    comboLay->addWidget(archLabel, 1, 0);
    mArchiveDefinitionCB = new QComboBox;
    comboLay->addWidget(mArchiveDefinitionCB, 1, 1);
    fileGrpLay->addLayout(comboLay);

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
    connect(mArchiveDefinitionCB, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &CryptoOperationsConfigWidget::changed);
    connect(mPGPFileExtCB, &QCheckBox::toggled, this, &CryptoOperationsConfigWidget::changed);
    connect(mAutoDecryptVerifyCB, &QCheckBox::toggled, this, &CryptoOperationsConfigWidget::changed);
    connect(mASCIIArmorCB, &QCheckBox::toggled, this, &CryptoOperationsConfigWidget::changed);
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

    if (mArchiveDefinitionCB->count()) {
        mArchiveDefinitionCB->setCurrentIndex(0);
    }
}

Q_DECLARE_METATYPE(std::shared_ptr<Kleo::ChecksumDefinition>)

void CryptoOperationsConfigWidget::load()
{

    const EMailOperationsPreferences emailPrefs;
    mQuickSignCB   ->setChecked(emailPrefs.quickSignEMail());
    mQuickEncryptCB->setChecked(emailPrefs.quickEncryptEMail());

    const FileOperationsPreferences filePrefs;
    mPGPFileExtCB->setChecked(filePrefs.usePGPFileExt());
    mAutoDecryptVerifyCB->setChecked(filePrefs.autoDecryptVerify());
    mASCIIArmorCB->setChecked(filePrefs.addASCIIArmor());

    const std::vector< std::shared_ptr<ChecksumDefinition> > cds = ChecksumDefinition::getChecksumDefinitions();
    const std::shared_ptr<ChecksumDefinition> default_cd = ChecksumDefinition::getDefaultChecksumDefinition(cds);

    mChecksumDefinitionCB->clear();
    mArchiveDefinitionCB->clear();

    Q_FOREACH (const std::shared_ptr<ChecksumDefinition> &cd, cds) {
        mChecksumDefinitionCB->addItem(cd->label(), qVariantFromValue(cd));
        if (cd == default_cd) {
            mChecksumDefinitionCB->setCurrentIndex(mChecksumDefinitionCB->count() - 1);
        }
    }

    const QString ad_default_id = filePrefs.archiveCommand();

    // This is a weird hack but because we are a KCM we can't link
    // against ArchiveDefinition which pulls in loads of other classes.
    // So we do the parsing which archive definitions exist here ourself.
    if (KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("libkleopatrarc"))) {
        const QStringList groups = config->groupList().filter(QRegularExpression(QStringLiteral("^Archive Definition #")));
        Q_FOREACH (const QString &group, groups) {
            const KConfigGroup cGroup(config, group);
            const QString id = cGroup.readEntryUntranslated(QStringLiteral("id"));
            const QString name = cGroup.readEntry("Name");
            mArchiveDefinitionCB->addItem(name, QVariant(id));
            if (id == ad_default_id) {
                mArchiveDefinitionCB->setCurrentIndex(mArchiveDefinitionCB->count() - 1);
            }
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
    filePrefs.setAddASCIIArmor(mASCIIArmorCB->isChecked());

    const int idx = mChecksumDefinitionCB->currentIndex();
    if (idx >= 0) {
        const std::shared_ptr<ChecksumDefinition> cd = qvariant_cast< std::shared_ptr<ChecksumDefinition> >(mChecksumDefinitionCB->itemData(idx));
        ChecksumDefinition::setDefaultChecksumDefinition(cd);
    }

    const int aidx = mArchiveDefinitionCB->currentIndex();
    if (aidx >= 0) {
        const QString id = mArchiveDefinitionCB->itemData(aidx).toString();
        filePrefs.setArchiveCommand(id);
    }
    filePrefs.save();
}

