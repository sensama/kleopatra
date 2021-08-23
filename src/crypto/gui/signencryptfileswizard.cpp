/*  crypto/gui/signencryptfileswizard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "signencryptfileswizard.h"
#include "signencryptwidget.h"

#include "newresultpage.h"

#include <KLocalizedString>
#include <KSharedConfig>
#include <KColorScheme>
#include <KConfigGroup>
#include <KWindowConfig>

#include <KMessageBox>

#include "kleopatra_debug.h"
#include <Libkleo/GnuPG>
#include <Libkleo/Formatting>

#include <Libkleo/FileNameRequester>
#include <QWindow>
#include <QVBoxLayout>
#include <QWizardPage>
#include <QGroupBox>
#include <QLabel>
#include <QIcon>
#include <QCheckBox>


#include <gpgme++/key.h>


using namespace GpgME;
using namespace Kleo;
using namespace Kleo::Crypto::Gui;

enum Page {
    SigEncPageId,
    ResultPageId,

    NumPages
};

class FileNameRequesterWithIcon : public QWidget
{
    Q_OBJECT

public:
    explicit FileNameRequesterWithIcon(QDir::Filters filter, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto layout = new QHBoxLayout{this};
        layout->setContentsMargins(0, 0, 0, 0);
        mIconLabel = new QLabel{this};
        mRequester = new FileNameRequester{filter, this};
        mRequester->setExistingOnly(false);
        layout->addWidget(mIconLabel);
        layout->addWidget(mRequester);

        setFocusPolicy(mRequester->focusPolicy());
        setFocusProxy(mRequester);

        connect(mRequester, &FileNameRequester::fileNameChanged,
                this, &FileNameRequesterWithIcon::fileNameChanged);
    }

    void setIcon(const QIcon &icon)
    {
        mIconLabel->setPixmap(icon.pixmap(32, 32));
    }

    void setFileName(const QString &name)
    {
        mRequester->setFileName(name);
    }

    QString fileName() const
    {
        return mRequester->fileName();
    }

Q_SIGNALS:
    void fileNameChanged(const QString &filename);

protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::ToolTipChange) {
            mRequester->setToolTip(toolTip());
        }
        return QWidget::event(e);
    }

private:
    QLabel *mIconLabel;
    FileNameRequester *mRequester;
};

class SigEncPage: public QWizardPage
{
    Q_OBJECT

public:
    explicit SigEncPage(QWidget *parent = nullptr)
        : QWizardPage(parent),
          mParent((SignEncryptFilesWizard *) parent),
          mWidget(new SignEncryptWidget),
          mOutLayout(new QVBoxLayout),
          mOutputLabel{nullptr},
          mArchive(false),
          mUseOutputDir(false)
    {
        setTitle(i18nc("@title", "Sign / Encrypt Files"));
        auto vLay = new QVBoxLayout(this);
        vLay->setContentsMargins(0, 0, 0, 0);
        mWidget->setSignAsText(i18nc("@option:check on SignEncryptPage",
                                     "&Sign as:"));
        mWidget->setEncryptForMeText(i18nc("@option:check on SignEncryptPage",
                                           "Encrypt for &me:"));
        mWidget->setEncryptForOthersText(i18nc("@option:check on SignEncryptPage",
                                               "Encrypt for &others:"));
        mWidget->setEncryptWithPasswordText(i18nc("@option:check on SignEncryptPage",
                                                  "Encrypt with &password. Anyone you share the password with can read the data."));
        vLay->addWidget(mWidget);
        connect(mWidget, &SignEncryptWidget::operationChanged, this,
                &SigEncPage::updateCommitButton);
        connect(mWidget, &SignEncryptWidget::keysChanged, this,
                &SigEncPage::updateFileWidgets);

        auto outputGrp = new QGroupBox(i18nc("@title:group", "Output"));
        outputGrp->setLayout(mOutLayout);

        mPlaceholderWidget = new QLabel(i18n("Please select an action."));
        mOutLayout->addWidget(mPlaceholderWidget);

        mOutputLabel = new QLabel(i18nc("@label on SignEncryptPage", "Output &files/folder:"));
        mOutLayout->addWidget(mOutputLabel);

        createRequesters(mOutLayout);

        mUseOutputDirChk = new QCheckBox(i18nc("@option:check on SignEncryptPage",
                                               "Encrypt / Sign &each file separately."));
        mUseOutputDirChk->setToolTip(i18nc("@info:tooltip",
                                           "Keep each file separate instead of creating an archive for all."));
        mOutLayout->addWidget(mUseOutputDirChk);
        connect (mUseOutputDirChk, &QCheckBox::toggled, this, [this] (bool state) {
                    mUseOutputDir = state;
                    mArchive = !mUseOutputDir;
                    updateFileWidgets();
                });

        vLay->addWidget(outputGrp);

        updateCommitButton(mWidget->currentOp());

        setMinimumHeight(300);
    }

    void setEncryptionPreset(bool value)
    {
        mWidget->setEncryptionChecked(value);
    }

    void setSigningPreset(bool value)
    {
        mWidget->setSigningChecked(value);
    }

    bool isComplete() const override
    {
        return !mWidget->currentOp().isNull();
    }

    int nextId() const override
    {
        return ResultPageId;
    }

    void initializePage() override
    {
        setCommitPage(true);
    }

    void setArchiveForced(bool archive)
    {
        mArchive = archive;
        setArchiveMutable(!archive);
    }

    void setArchiveMutable(bool archive)
    {
        mUseOutputDirChk->setVisible(archive);
        if (archive) {
            const KConfigGroup archCfg(KSharedConfig::openConfig(), "SignEncryptFilesWizard");
            mUseOutputDirChk->setChecked(archCfg.readEntry("LastUseOutputDir", false));
        } else {
            mUseOutputDirChk->setChecked(false);
        }
    }

    bool validatePage() override
    {
        bool sign = !mWidget->signKey().isNull();
        bool encrypt = !mWidget->selfKey().isNull() || !mWidget->recipients().empty();
        if (!mWidget->validate()) {
            return false;
        }
        mWidget->saveOwnKeys();
        if (mUseOutputDirChk->isVisible()) {
            KConfigGroup archCfg(KSharedConfig::openConfig(), "SignEncryptFilesWizard");
            archCfg.writeEntry("LastUseOutputDir", mUseOutputDir);
        }

        if (sign && !encrypt && mArchive) {
            return KMessageBox::warningContinueCancel(this,
            xi18nc("@info",
            "<para>Archiving in combination with sign-only currently requires what are known as opaque signatures - "
            "unlike detached ones, these embed the content in the signature.</para>"
            "<para>This format is rather unusual. You might want to archive the files separately, "
            "and then sign the archive as one file with Kleopatra.</para>"
            "<para>Future versions of Kleopatra are expected to also support detached signatures in this case.</para>"),
            i18nc("@title:window", "Unusual Signature Warning"),
            KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
            QStringLiteral("signencryptfileswizard-archive+sign-only-warning"))
            == KMessageBox::Continue;
        } else if (sign && !encrypt) {
            return true;
        }

        if (!mWidget->selfKey().isNull()) {
            return true;
        }
        bool hasSecret = false;
        Q_FOREACH (const Key k, mWidget->recipients()) {
            if (k.hasSecret()) {
                hasSecret = true;
                break;
            }
        }
        if (!hasSecret && !mWidget->encryptSymmetric()) {
            if (KMessageBox::warningContinueCancel(this,
            xi18nc("@info",
            "<para>None of the recipients you are encrypting to seems to be your own.</para>"
            "<para>This means that you will not be able to decrypt the data anymore, once encrypted.</para>"
            "<para>Do you want to continue, or cancel to change the recipient selection?</para>"),
            i18nc("@title:window", "Encrypt-To-Self Warning"),
            KStandardGuiItem::cont(),
            KStandardGuiItem::cancel(),
            QStringLiteral("warn-encrypt-to-non-self"), KMessageBox::Notify | KMessageBox::Dangerous)
            == KMessageBox::Cancel) {
                return false;
            }
        }
        return true;
    }

    std::vector<Key> recipients() const
    {
        return mWidget->recipients();
    }

    /* In the future we might find a usecase for multiple
     * signers */
    std::vector<Key> signers() const
    {
        const Key k = mWidget->signKey();
        if (!k.isNull()) {
            return {k};
        }
        return {};
    }

private:
    void createRequesters(QBoxLayout *lay) {
        static const QMap <int, QString> icons = {
            { SignEncryptFilesWizard::SignatureCMS, QStringLiteral("document-sign") },
            { SignEncryptFilesWizard::SignaturePGP, QStringLiteral("document-sign") },
            { SignEncryptFilesWizard::CombinedPGP,  QStringLiteral("document-edit-sign-encrypt") },
            { SignEncryptFilesWizard::EncryptedPGP, QStringLiteral("document-encrypt") },
            { SignEncryptFilesWizard::EncryptedCMS, QStringLiteral("document-encrypt") },
            { SignEncryptFilesWizard::Directory,    QStringLiteral("folder") }
        };
        static const QMap <int, QString> toolTips = {
            { SignEncryptFilesWizard::SignatureCMS, i18n("The S/MIME signature.") },
            { SignEncryptFilesWizard::SignaturePGP, i18n("The signature.") },
            { SignEncryptFilesWizard::CombinedPGP,  i18n("The signed and encrypted file.") },
            { SignEncryptFilesWizard::EncryptedPGP, i18n("The encrypted file.") },
            { SignEncryptFilesWizard::EncryptedCMS, i18n("The S/MIME encrypted file.") },
            { SignEncryptFilesWizard::Directory,    i18n("Output directory.") }
        };

        if (!mRequesters.empty()) {
            return;
        }
        for (auto kind : icons.keys()) {
            auto requesterWithIcon = new FileNameRequesterWithIcon{
                kind == SignEncryptFilesWizard::Directory ? QDir::Dirs : QDir::Files, this};
            requesterWithIcon->setIcon(QIcon::fromTheme(icons[kind]));
            requesterWithIcon->setToolTip(toolTips[kind]);
            lay->addWidget(requesterWithIcon);

            connect(requesterWithIcon, &FileNameRequesterWithIcon::fileNameChanged, this,
                    [this, kind](const QString &newName) {
                        mOutNames[kind] = newName;
                    });

            mRequesters.insert(kind, requesterWithIcon);
        }
    }

public:
    void setOutputNames(const QMap<int, QString> &names) {
        Q_ASSERT(mOutNames.isEmpty());
        for (auto it = std::begin(names); it != std::end(names); ++it) {
            mRequesters.value(it.key())->setFileName(it.value());
        }
        mOutNames = names;
        updateFileWidgets();
    }

    QMap <int, QString> outputNames() const {
        if (!mUseOutputDir) {
            auto ret = mOutNames;
            ret.remove(SignEncryptFilesWizard::Directory);
            return ret;
        }
        return mOutNames;
    }

    bool encryptSymmetric() const
    {
        return mWidget->encryptSymmetric();
    }

private Q_SLOTS:
    void updateCommitButton(const QString &label)
    {
        auto btn = mParent->button(QWizard::CommitButton);
        if (!label.isEmpty()) {
            mParent->setButtonText(QWizard::CommitButton, label);
            if (Kleo::gpgComplianceP("de-vs")) {
                bool de_vs = mWidget->isDeVsAndValid();
                btn->setIcon(QIcon::fromTheme(de_vs
                                             ? QStringLiteral("security-high")
                                             : QStringLiteral("security-medium")));
                btn->setStyleSheet(QStringLiteral("background-color: ") + (de_vs
                                   ? KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::PositiveBackground).color().name()
                                   : KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::NegativeBackground).color().name()));
                mParent->setLabelText(de_vs
                        ? i18nc("%1 is a placeholder for the name of a compliance mode. E.g. NATO RESTRICTED compliant or VS-NfD compliant",
                            "%1 communication possible.", Formatting::deVsString())
                        : i18nc("%1 is a placeholder for the name of a compliance mode. E.g. NATO RESTRICTED compliant or VS-NfD compliant",
                            "%1 communication not possible.", Formatting::deVsString()));
            }
        } else {
            mParent->setButtonText(QWizard::CommitButton, i18n("Next"));
            btn->setIcon(QIcon());
            btn->setStyleSheet(QString());
        }
        Q_EMIT completeChanged();
    }

    void updateFileWidgets()
    {
        if (mRequesters.isEmpty()) {
            return;
        }
        const std::vector<Key> recipients = mWidget->recipients();
        const Key sigKey = mWidget->signKey();
        const bool pgp = mWidget->encryptSymmetric() ||
                         std::any_of(std::cbegin(recipients), std::cend(recipients),
                                     [](const auto &k) { return k.protocol() == Protocol::OpenPGP; });
        const bool cms = std::any_of(std::cbegin(recipients), std::cend(recipients),
                                     [](const auto &k) { return k.protocol() == Protocol::CMS; });

        mOutLayout->setEnabled(false);
        if (cms || pgp || !sigKey.isNull()) {
            mPlaceholderWidget->setVisible(false);
            mOutputLabel->setVisible(true);
            mRequesters[SignEncryptFilesWizard::SignatureCMS]->setVisible(!mUseOutputDir && sigKey.protocol() == Protocol::CMS);
            mRequesters[SignEncryptFilesWizard::EncryptedCMS]->setVisible(!mUseOutputDir && cms);
            mRequesters[SignEncryptFilesWizard::CombinedPGP]->setVisible(!mUseOutputDir && sigKey.protocol() == Protocol::OpenPGP && pgp);
            mRequesters[SignEncryptFilesWizard::EncryptedPGP]->setVisible(!mUseOutputDir && sigKey.protocol() != Protocol::OpenPGP && pgp);
            mRequesters[SignEncryptFilesWizard::SignaturePGP]->setVisible(!mUseOutputDir && sigKey.protocol() == Protocol::OpenPGP && !pgp);
            mRequesters[SignEncryptFilesWizard::Directory]->setVisible(mUseOutputDir);
            auto firstNotHidden = std::find_if(std::cbegin(mRequesters), std::cend(mRequesters),
                                               [](auto w) { return !w->isHidden(); });
            mOutputLabel->setBuddy(*firstNotHidden);
        } else {
            mPlaceholderWidget->setVisible(true);
            mOutputLabel->setVisible(false);
            std::for_each(std::cbegin(mRequesters), std::cend(mRequesters),
                          [](auto w) { w->setVisible(false); });
            mOutputLabel->setBuddy(nullptr);
        }
        mOutLayout->setEnabled(true);
    }

private:
    SignEncryptFilesWizard *mParent;
    SignEncryptWidget *mWidget;
    QMap <int, QString> mOutNames;
    QMap <int, FileNameRequesterWithIcon *> mRequesters;
    QVBoxLayout *mOutLayout;
    QWidget *mPlaceholderWidget;
    QCheckBox *mUseOutputDirChk;
    QLabel *mOutputLabel;
    bool mArchive;
    bool mUseOutputDir;
};

class ResultPage : public NewResultPage
{
    Q_OBJECT

public:
    explicit ResultPage(QWidget *parent = nullptr)
        : NewResultPage(parent),
          mParent((SignEncryptFilesWizard *) parent)
    {
        setTitle(i18nc("@title", "Results"));
        setSubTitle(i18nc("@title",
                          "Status and progress of the crypto operations is shown here."));
    }

    void initializePage() override
    {
        mParent->setLabelText(QString());
    }

private:
    SignEncryptFilesWizard *mParent;
};

SignEncryptFilesWizard::SignEncryptFilesWizard(QWidget *parent, Qt::WindowFlags f)
    : QWizard(parent, f)
{
    readConfig();

    bool de_vs = Kleo::gpgComplianceP("de-vs");
#ifdef Q_OS_WIN
    // Enforce modern style to avoid vista style ugliness.
    setWizardStyle(QWizard::ModernStyle);
#endif
    mSigEncPage = new SigEncPage(this);
    mResultPage = new ResultPage(this);

    connect(this, &QWizard::currentIdChanged, this,
            &SignEncryptFilesWizard::slotCurrentIdChanged);
    setPage(SigEncPageId, mSigEncPage);
    setPage(ResultPageId, mResultPage);
    setOptions(QWizard::IndependentPages |
               (de_vs ? QWizard::HaveCustomButton1 : QWizard::WizardOption(0)) |
               QWizard::NoBackButtonOnLastPage |
               QWizard::NoBackButtonOnStartPage);

    if (de_vs) {
        /* We use a custom button to display a label next to the
           buttons.  */
        auto btn = button(QWizard::CustomButton1);
        /* We style the button so that it looks and acts like a
           label.  */
        btn->setStyleSheet(QStringLiteral("border: none"));
        btn->setFocusPolicy(Qt::NoFocus);
    }
}

void SignEncryptFilesWizard::setLabelText(const QString &label)
{
    button(QWizard::CommitButton)->setToolTip(label);
    setButtonText(QWizard::CustomButton1, label);
}

void SignEncryptFilesWizard::slotCurrentIdChanged(int id)
{
    if (id == ResultPageId) {
        Q_EMIT operationPrepared();
    }
}

SignEncryptFilesWizard::~SignEncryptFilesWizard()
{
    qCDebug(KLEOPATRA_LOG);
    writeConfig();
}

void SignEncryptFilesWizard::setSigningPreset(bool preset)
{
    mSigEncPage->setSigningPreset(preset);
}

void SignEncryptFilesWizard::setSigningUserMutable(bool mut)
{
    if (mut == mSigningUserMutable) {
        return;
    }
    mSigningUserMutable = mut;
}

void SignEncryptFilesWizard::setEncryptionPreset(bool preset)
{
    mSigEncPage->setEncryptionPreset(preset);
}

void SignEncryptFilesWizard::setEncryptionUserMutable(bool mut)
{
    if (mut == mEncryptionUserMutable) {
        return;
    }
    mEncryptionUserMutable = mut;
}

void SignEncryptFilesWizard::setArchiveForced(bool archive)
{
    mSigEncPage->setArchiveForced(archive);
}

void SignEncryptFilesWizard::setArchiveMutable(bool archive)
{
    mSigEncPage->setArchiveMutable(archive);
}

std::vector<Key> SignEncryptFilesWizard::resolvedRecipients() const
{
    return mSigEncPage->recipients();
}

std::vector<Key> SignEncryptFilesWizard::resolvedSigners() const
{
    return mSigEncPage->signers();
}

void SignEncryptFilesWizard::setTaskCollection(const std::shared_ptr<Kleo::Crypto::TaskCollection> &coll)
{
    mResultPage->setTaskCollection(coll);
}

void SignEncryptFilesWizard::setOutputNames(const QMap<int, QString> &map) const
{
    mSigEncPage->setOutputNames(map);
}

QMap<int, QString> SignEncryptFilesWizard::outputNames() const
{
    return mSigEncPage->outputNames();
}

bool SignEncryptFilesWizard::encryptSymmetric() const
{
    return mSigEncPage->encryptSymmetric();
}

void SignEncryptFilesWizard::readConfig()
{
    winId(); // ensure there's a window created

    // set default window size
    windowHandle()->resize(640, 480);

    // restore size from config file
    KConfigGroup cfgGroup(KSharedConfig::openConfig(), "SignEncryptFilesWizard");
    KWindowConfig::restoreWindowSize(windowHandle(), cfgGroup);

    // NOTICE: QWindow::setGeometry() does NOT impact the backing QWidget geometry even if the platform
    // window was created -> QTBUG-40584. We therefore copy the size here.
    // TODO: remove once this was resolved in QWidget QPA
    resize(windowHandle()->size());
}

void SignEncryptFilesWizard::writeConfig()
{
    KConfigGroup cfgGroup(KSharedConfig::openConfig(), "SignEncryptFilesWizard");
    KWindowConfig::saveWindowSize(windowHandle(), cfgGroup);
    cfgGroup.sync();
}


#include "signencryptfileswizard.moc"
