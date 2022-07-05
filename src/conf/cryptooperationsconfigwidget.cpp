/*
    cryptooperationsconfigwidget.cpp

    This file is part of kleopatra, the KDE key manager
    SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config-kleopatra.h>

#include "cryptooperationsconfigwidget.h"
#include "kleopatra_debug.h"

#include "emailoperationspreferences.h"
#include "fileoperationspreferences.h"
#include "settings.h"

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
    : QWidget{p, f}
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

    QDir datadir(QString::fromUtf8(GpgME::dirInfo("datadir")) + QStringLiteral("/../doc/gnupg/examples"));
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

    connect(gpgconf, &QProcess::finished,
            this, [this, gpgconf, profile] () {
        mApplyBtn->setEnabled(true);
        if (gpgconf->exitStatus() != QProcess::NormalExit) {
            KMessageBox::error(this, QStringLiteral("<pre>%1</pre>").arg(QString::fromUtf8(gpgconf->readAll())));
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
    const Settings settings;
    if (settings.profilesDisabled() || GpgME::engineInfo(GpgME::GpgEngine).engineVersion() < "2.1.20" || !layout) {
        //  Profile support is new in 2.1.20
        qCDebug(KLEOPATRA_LOG) << "Profile settings disabled";
        return;
    }
    QDir datadir(QString::fromUtf8(GpgME::dirInfo("datadir")) + QStringLiteral("/../doc/gnupg/examples"));

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

    connect(combo, &QComboBox::currentTextChanged, this, [this] (const QString &text) {
        mApplyBtn->setEnabled(!text.isEmpty());
    });
}

void CryptoOperationsConfigWidget::setupGui()
{
    auto baseLay = new QVBoxLayout(this);
    baseLay->setContentsMargins(0, 0, 0, 0);

    auto mailGrp = new QGroupBox(i18n("EMail Operations"));
    auto mailGrpLayout = new QVBoxLayout;
    mQuickSignCB = new QCheckBox(i18n("Don't confirm signing certificate if there is only one valid certificate for the identity"));
    mQuickEncryptCB = new QCheckBox(i18n("Don't confirm encryption certificates if there is exactly one valid certificate for each recipient"));
    mailGrpLayout->addWidget(mQuickSignCB);
    mailGrpLayout->addWidget(mQuickEncryptCB);
    mailGrp->setLayout(mailGrpLayout);
    baseLay->addWidget(mailGrp);

    auto fileGrp = new QGroupBox(i18n("File Operations"));
    auto fileGrpLay = new QVBoxLayout;
    mPGPFileExtCB = new QCheckBox(i18n(R"(Create OpenPGP encrypted files with ".pgp" file extensions instead of ".gpg")"));
    mASCIIArmorCB = new QCheckBox(i18n("Create signed or encrypted files as text files."));
    mASCIIArmorCB->setToolTip(i18nc("@info", "Set this option to encode encrypted or signed files as base64 encoded text. "
                                             "So that they can be opened with an editor or sent in a mail body. "
                                             "This will increase file size by one third."));
    mAutoDecryptVerifyCB = new QCheckBox(i18n("Automatically start operation based on input detection for decrypt/verify."));
    mAutoExtractArchivesCB = new QCheckBox(i18n("Automatically extract file archives after decryption"));
    mTmpDirCB = new QCheckBox(i18n("Create temporary decrypted files in the folder of the encrypted file."));
    mTmpDirCB->setToolTip(i18nc("@info", "Set this option to avoid using the users temporary directory."));
    mSymmetricOnlyCB = new QCheckBox(i18n("Use symmetric encryption only."));
    mSymmetricOnlyCB->setToolTip(i18nc("@info", "Set this option to disable public key encryption."));

    fileGrpLay->addWidget(mPGPFileExtCB);
    fileGrpLay->addWidget(mAutoDecryptVerifyCB);
    fileGrpLay->addWidget(mAutoExtractArchivesCB);
    fileGrpLay->addWidget(mASCIIArmorCB);
    fileGrpLay->addWidget(mTmpDirCB);
    fileGrpLay->addWidget(mSymmetricOnlyCB);

    auto comboLay = new QGridLayout;

    mChecksumDefinitionCB.createWidgets(this);
    mChecksumDefinitionCB.label()->setText(i18n("Checksum program to use when creating checksum files:"));
    comboLay->addWidget(mChecksumDefinitionCB.label(), 0, 0);
    comboLay->addWidget(mChecksumDefinitionCB.widget(), 0, 1);

    mArchiveDefinitionCB.createWidgets(this);
    mArchiveDefinitionCB.label()->setText(i18n("Archive command to use when archiving files:"));
    comboLay->addWidget(mArchiveDefinitionCB.label(), 1, 0);
    comboLay->addWidget(mArchiveDefinitionCB.widget(), 1, 1);

    fileGrpLay->addLayout(comboLay);

    fileGrp->setLayout(fileGrpLay);
    baseLay->addWidget(fileGrp);

    setupProfileGui(baseLay);

    baseLay->addStretch(1);

    for (auto cb : findChildren<QCheckBox *>()) {
        connect(cb, &QCheckBox::toggled, this, &CryptoOperationsConfigWidget::changed);
    }
    for (auto combo : findChildren<QComboBox *>()) {
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, &CryptoOperationsConfigWidget::changed);
    }
}

CryptoOperationsConfigWidget::~CryptoOperationsConfigWidget() {}

void CryptoOperationsConfigWidget::defaults()
{
    EMailOperationsPreferences emailPrefs;
    emailPrefs.setQuickSignEMail(emailPrefs.findItem(QStringLiteral("QuickSignEMail"))->getDefault().toBool());
    emailPrefs.setQuickEncryptEMail(emailPrefs.findItem(QStringLiteral("QuickEncryptEMail"))->getDefault().toBool());

    FileOperationsPreferences filePrefs;
    filePrefs.setUsePGPFileExt(filePrefs.findItem(QStringLiteral("UsePGPFileExt"))->getDefault().toBool());
    filePrefs.setAutoDecryptVerify(filePrefs.findItem(QStringLiteral("AutoDecryptVerify"))->getDefault().toBool());
    filePrefs.setAutoExtractArchives(filePrefs.findItem(QStringLiteral("AutoExtractArchives"))->getDefault().toBool());
    filePrefs.setAddASCIIArmor(filePrefs.findItem(QStringLiteral("AddASCIIArmor"))->getDefault().toBool());
    filePrefs.setDontUseTmpDir(filePrefs.findItem(QStringLiteral("DontUseTmpDir"))->getDefault().toBool());
    filePrefs.setSymmetricEncryptionOnly(filePrefs.findItem(QStringLiteral("SymmetricEncryptionOnly"))->getDefault().toBool());
    filePrefs.setArchiveCommand(filePrefs.findItem(QStringLiteral("ArchiveCommand"))->getDefault().toString());

    Settings settings;
    settings.setChecksumDefinitionId(settings.findItem(QStringLiteral("ChecksumDefinitionId"))->getDefault().toString());

    load(emailPrefs, filePrefs, settings);
}

void CryptoOperationsConfigWidget::load(const Kleo::EMailOperationsPreferences &emailPrefs,
                                        const Kleo::FileOperationsPreferences &filePrefs,
                                        const Kleo::Settings &settings)
{
    mQuickSignCB->setChecked(emailPrefs.quickSignEMail());
    mQuickSignCB->setEnabled(!emailPrefs.isImmutable(QStringLiteral("QuickSignEMail")));
    mQuickEncryptCB->setChecked(emailPrefs.quickEncryptEMail());
    mQuickEncryptCB->setEnabled(!emailPrefs.isImmutable(QStringLiteral("QuickEncryptEMail")));

    mPGPFileExtCB->setChecked(filePrefs.usePGPFileExt());
    mPGPFileExtCB->setEnabled(!filePrefs.isImmutable(QStringLiteral("UsePGPFileExt")));
    mAutoDecryptVerifyCB->setChecked(filePrefs.autoDecryptVerify());
    mAutoDecryptVerifyCB->setEnabled(!filePrefs.isImmutable(QStringLiteral("AutoDecryptVerify")));
    mAutoExtractArchivesCB->setChecked(filePrefs.autoExtractArchives());
    mAutoExtractArchivesCB->setEnabled(!filePrefs.isImmutable(QStringLiteral("AutoExtractArchives")));
    mASCIIArmorCB->setChecked(filePrefs.addASCIIArmor());
    mASCIIArmorCB->setEnabled(!filePrefs.isImmutable(QStringLiteral("AddASCIIArmor")));
    mTmpDirCB->setChecked(filePrefs.dontUseTmpDir());
    mTmpDirCB->setEnabled(!filePrefs.isImmutable(QStringLiteral("DontUseTmpDir")));
    mSymmetricOnlyCB->setChecked(filePrefs.symmetricEncryptionOnly());
    mSymmetricOnlyCB->setEnabled(!filePrefs.isImmutable(QStringLiteral("SymmetricEncryptionOnly")));

    const auto defaultChecksumDefinitionId = settings.checksumDefinitionId();
    {
        const auto index = mChecksumDefinitionCB.widget()->findData(defaultChecksumDefinitionId);
        if (index >= 0) {
            mChecksumDefinitionCB.widget()->setCurrentIndex(index);
        } else {
            qCWarning(KLEOPATRA_LOG) << "No checksum definition found with id" << defaultChecksumDefinitionId;
        }
    }
    mChecksumDefinitionCB.setEnabled(!settings.isImmutable(QStringLiteral("ChecksumDefinitionId")));

    const auto ad_default_id = filePrefs.archiveCommand();
    {
        const auto index = mArchiveDefinitionCB.widget()->findData(ad_default_id);
        if (index >= 0) {
            mArchiveDefinitionCB.widget()->setCurrentIndex(index);
        } else {
            qCWarning(KLEOPATRA_LOG) << "No archive definition found with id" << ad_default_id;
        }
    }
    mArchiveDefinitionCB.setEnabled(!filePrefs.isImmutable(QStringLiteral("ArchiveCommand")));
}


void CryptoOperationsConfigWidget::load()
{
    mChecksumDefinitionCB.widget()->clear();
    const auto cds = ChecksumDefinition::getChecksumDefinitions();
    for (const std::shared_ptr<ChecksumDefinition> &cd : cds) {
        mChecksumDefinitionCB.widget()->addItem(cd->label(), QVariant{cd->id()});
    }

    // This is a weird hack but because we are a KCM we can't link
    // against ArchiveDefinition which pulls in loads of other classes.
    // So we do the parsing which archive definitions exist here ourself.
    mArchiveDefinitionCB.widget()->clear();
    if (KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("libkleopatrarc"))) {
        const QStringList groups = config->groupList().filter(QRegularExpression(QStringLiteral("^Archive Definition #")));
        for (const QString &group : groups) {
            const KConfigGroup cGroup(config, group);
            const QString id = cGroup.readEntryUntranslated(QStringLiteral("id"));
            const QString name = cGroup.readEntry("Name");
            mArchiveDefinitionCB.widget()->addItem(name, QVariant(id));
        }
    }

    load(EMailOperationsPreferences{}, FileOperationsPreferences{}, Settings{});
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
    filePrefs.setAutoExtractArchives(mAutoExtractArchivesCB->isChecked());
    filePrefs.setAddASCIIArmor(mASCIIArmorCB->isChecked());
    filePrefs.setDontUseTmpDir(mTmpDirCB->isChecked());
    filePrefs.setSymmetricEncryptionOnly(mSymmetricOnlyCB->isChecked());

    Settings settings;
    const int idx = mChecksumDefinitionCB.widget()->currentIndex();
    if (idx >= 0) {
        const auto id = mChecksumDefinitionCB.widget()->itemData(idx).toString();
        settings.setChecksumDefinitionId(id);
    }
    settings.save();

    const int aidx = mArchiveDefinitionCB.widget()->currentIndex();
    if (aidx >= 0) {
        const QString id = mArchiveDefinitionCB.widget()->itemData(aidx).toString();
        filePrefs.setArchiveCommand(id);
    }
    filePrefs.save();
}
