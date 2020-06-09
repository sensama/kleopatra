/*
    cryptooperationsconfigwidget.cpp

    This file is part of kleopatra, the KDE key manager
    Copyright (c) 2010 Klarälvdalens Datakonsult AB
    2016 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

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
#include "kleopatra_debug.h"

#include "emailoperationspreferences.h"
#include "fileoperationspreferences.h"

#include <Libkleo/ChecksumDefinition>
#include <Libkleo/KeyFilterManager>

#include <QGpgME/Protocol>
#include <QGpgME/CryptoConfig>

#include <gpgme++/context.h>
#include <gpgme++/engineinfo.h>

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KMessageBox>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QProcess>
#include <QVBoxLayout>
#include <QLabel>
#include <QRegularExpression>

#include <memory>

using namespace Kleo;
using namespace Kleo::Config;

CryptoOperationsConfigWidget::CryptoOperationsConfigWidget(QWidget *p, Qt::WindowFlags f)
    : QWidget(p, f),
      mApplyBtn(nullptr)
{
    setupGui();
}

static void resetDefaults()
{
    auto config = QGpgME::cryptoConfig();

    if (!config) {
        qCWarning(KLEOPATRA_LOG) << "Failed to obtain config";
        return;
    }

    const QStringList componentList = config->componentList();
    for (const auto &compName: componentList) {
        auto comp = config->component(compName);
        if (!comp) {
            qCWarning(KLEOPATRA_LOG) << "Failed to find component:" << comp;
            return;
        }
        const QStringList groupList = comp->groupList();
        for (const auto &grpName: groupList) {
            auto grp = comp->group(grpName);
            if (!grp) {
                qCWarning(KLEOPATRA_LOG) << "Failed to find group:" << grp << "in component:" << compName;
                return;
            }
            const QStringList entries = grp->entryList();
            for (const auto &entryName: entries) {
                auto entry = grp->entry(entryName);
                if (!entry) {
                    qCWarning(KLEOPATRA_LOG) << "Failed to find entry:" << entry << "in group:"<< grp << "in component:" << compName;
                    return;
                }
                entry->resetToDefault();
            }
        }
    }
    config->sync(true);
    return;
}

void CryptoOperationsConfigWidget::applyProfile(const QString &profile)
{
    if (profile.isEmpty()) {
        return;
    }
    qCDebug(KLEOPATRA_LOG) << "Applying profile " << profile;

    if (profile == i18n("default")) {
        if (KMessageBox::warningYesNo(
                          this,
                          i18n("This means that every configuration option of the GnuPG System will be reset to its default."),
                          i18n("Apply profile"),
                          KStandardGuiItem::apply(),
                          KStandardGuiItem::no()) != KMessageBox::Yes) {
            return;
        }
        resetDefaults();
        KeyFilterManager::instance()->reload();
        return;
    }

    mApplyBtn->setEnabled(false);

    QDir datadir(QString::fromLocal8Bit(GpgME::dirInfo("datadir")) + QStringLiteral("/../doc/gnupg/examples"));
    const auto path = datadir.filePath(profile + QStringLiteral(".prf"));

    auto gpgconf = new QProcess;
    const auto ei = GpgME::engineInfo(GpgME::GpgConfEngine);
    Q_ASSERT (ei.fileName());
    gpgconf->setProgram(QFile::decodeName(ei.fileName()));
    gpgconf->setProcessChannelMode(QProcess::MergedChannels);
    gpgconf->setArguments(QStringList() << QStringLiteral("--runtime")
                                        << QStringLiteral("--apply-profile")
                                        << path);

    qDebug() << "Starting" << ei.fileName() << "with args" << gpgconf->arguments();

    connect(gpgconf, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this, gpgconf, profile] () {
        mApplyBtn->setEnabled(true);
        if (gpgconf->exitStatus() != QProcess::NormalExit) {
            KMessageBox::error(this, QStringLiteral("<pre>%1</pre>").arg(QString::fromLocal8Bit(gpgconf->readAll())));
            delete gpgconf;
            return;
        }
        delete gpgconf;
        KMessageBox::information(this,
                         i18nc("%1 is the name of the profile",
                               "The configuration profile \"%1\" was applied.", profile),
                         i18n("GnuPG Profile - Kleopatra"));
        auto config = QGpgME::cryptoConfig();
        if (config) {
            config->clear();
        }
        KeyFilterManager::instance()->reload();
    });
    gpgconf->start();
}

// Get a list of available profile files and add a configuration
// group if there are any.
void CryptoOperationsConfigWidget::setupProfileGui(QBoxLayout *layout)
{
    qCDebug(KLEOPATRA_LOG) << "Engine version ";
    if (GpgME::engineInfo(GpgME::GpgEngine).engineVersion() < "2.1.20" || !layout) {
        //  Profile support is new in 2.1.20
        qCDebug(KLEOPATRA_LOG) << "Engine version false";
        return;
    }
    QDir datadir(QString::fromLocal8Bit(GpgME::dirInfo("datadir")) + QStringLiteral("/../doc/gnupg/examples"));

    if (!datadir.exists()) {
        qCDebug(KLEOPATRA_LOG) << "Failed to find gnupg's example profile directory" << datadir.path();
        return;
    }

    const auto profiles = datadir.entryInfoList(QStringList() << QStringLiteral("*.prf"), QDir::Readable | QDir::Files, QDir::Name);

    if (profiles.isEmpty()) {
        qCDebug(KLEOPATRA_LOG) << "Failed to find any profiles in: " << datadir.path();
        return;
    }


    auto genGrp = new QGroupBox(i18nc("@title", "General Operations"));
    auto profLayout = new QHBoxLayout;
    genGrp->setLayout(profLayout);
    layout->addWidget(genGrp);
    auto profLabel = new QLabel(i18n("Activate GnuPG Profile:"));
    profLabel->setToolTip(i18n("A profile consists of various settings that can apply to multiple components of the GnuPG system."));

    auto combo = new QComboBox;
    profLabel->setBuddy(combo);

    // Add an empty Item to avoid the impression that this GUI element
    // shows the currently selected profile.
    combo->addItem(QString());

    // We don't translate "default" here because the other profile names are
    // also not translated as they are taken directly from file.
    combo->addItem(i18n("default"));
    for (const auto &profile: profiles) {
        combo->addItem(profile.baseName());
    }

    mApplyBtn = new QPushButton(i18n("Apply"));

    mApplyBtn->setEnabled(false);

    profLayout->addWidget(profLabel);
    profLayout->addWidget(combo);
    profLayout->addWidget(mApplyBtn);
    profLayout->addStretch(1);

    connect(mApplyBtn, &QPushButton::clicked, this, [this, combo] () {
        applyProfile(combo->currentText());
    });

    connect(combo, QOverload<const QString &>::of(&QComboBox::currentTextChanged), this, [this] (const QString &text) {
        mApplyBtn->setEnabled(!text.isEmpty());
    });
}

void CryptoOperationsConfigWidget::setupGui()
{
    QVBoxLayout *baseLay = new QVBoxLayout(this);
    baseLay->setContentsMargins(0, 0, 0, 0);

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
    mTmpDirCB = new QCheckBox(i18n("Create temporary decrypted files in the folder of the encrypted file."));
    mTmpDirCB->setToolTip(i18nc("@info", "Set this option to avoid using the users temporary directory."));

    fileGrpLay->addWidget(mPGPFileExtCB);
    fileGrpLay->addWidget(mAutoDecryptVerifyCB);
    fileGrpLay->addWidget(mASCIIArmorCB);
    fileGrpLay->addWidget(mTmpDirCB);

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

    setupProfileGui(baseLay);

    baseLay->addStretch(1);


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
    connect(mTmpDirCB, &QCheckBox::toggled, this, &CryptoOperationsConfigWidget::changed);
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
    mTmpDirCB->setChecked(filePrefs.dontUseTmpDir());

    const std::vector< std::shared_ptr<ChecksumDefinition> > cds = ChecksumDefinition::getChecksumDefinitions();
    const std::shared_ptr<ChecksumDefinition> default_cd = ChecksumDefinition::getDefaultChecksumDefinition(cds);

    mChecksumDefinitionCB->clear();
    mArchiveDefinitionCB->clear();

    for (const std::shared_ptr<ChecksumDefinition> &cd : cds) {
        mChecksumDefinitionCB->addItem(cd->label(), QVariant::fromValue(cd));
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
        for (const QString &group : groups) {
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
    filePrefs.setDontUseTmpDir(mTmpDirCB->isChecked());

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
