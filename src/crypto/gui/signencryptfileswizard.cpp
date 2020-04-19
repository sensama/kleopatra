/*  crypto/gui/signencryptfileswizard.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2009 Klarälvdalens Datakonsult AB
    2016 by Bundesamt für Sicherheit in der Informationstechnik
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
#include <utils/gnupg-helper.h>

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

class SigEncPage: public QWizardPage
{
    Q_OBJECT

public:
    explicit SigEncPage(QWidget *parent = nullptr)
        : QWizardPage(parent),
          mParent((SignEncryptFilesWizard *) parent),
          mWidget(new SignEncryptWidget),
          mOutLayout(new QVBoxLayout),
          mArchive(false),
          mUseOutputDir(false)
    {
        setTitle(i18nc("@title", "Sign / Encrypt Files"));
        auto vLay = new QVBoxLayout(this);
        vLay->setContentsMargins(0, 0, 0, 0);
        vLay->addWidget(mWidget);
        connect(mWidget, &SignEncryptWidget::operationChanged, this,
                &SigEncPage::updateCommitButton);
        connect(mWidget, &SignEncryptWidget::keysChanged, this,
                &SigEncPage::updateFileWidgets);
        updateCommitButton(mWidget->currentOp());

        auto outputGrp = new QGroupBox(i18n("Output"));
        outputGrp->setLayout(mOutLayout);

        mPlaceholderWidget = new QLabel(i18n("Please select an action."));
        mOutLayout->addWidget(mPlaceholderWidget);

        mUseOutputDirChk = new QCheckBox(i18n("Encrypt / Sign each file separately."));
        mUseOutputDirChk->setToolTip(i18nc("@info",
                                            "Keep each file separate instead of creating an archive for all."));

        mOutLayout->addWidget(mUseOutputDirChk);
        connect (mUseOutputDirChk, &QCheckBox::toggled, this, [this] (bool state) {
                    mUseOutputDir = state;
                    mArchive = !mUseOutputDir;
                    updateFileWidgets();
                });

        vLay->addWidget(outputGrp);
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

    QVector<Key> recipients() const
    {
        return mWidget->recipients();
    }

    /* In the future we might find a usecase for multiple
     * signers */
    QVector<Key> signers() const
    {
        QVector<Key> ret;
        const Key k = mWidget->signKey();
        if (!k.isNull()) {
            ret << k;
        }
        return ret;
    }

private:
    QWidget *createRequester(int forKind, QBoxLayout *lay) {
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

        FileNameRequester *req = new FileNameRequester(forKind == SignEncryptFilesWizard::Directory ?
                                                       QDir::Dirs : QDir::Files, this);
        req->setFileName(mOutNames[forKind]);
        QHBoxLayout *hLay = new QHBoxLayout;
        QLabel *iconLabel = new QLabel;
        QWidget *ret = new QWidget;
        iconLabel->setPixmap(QIcon::fromTheme(icons[forKind]).pixmap(32,32));
        hLay->addWidget(iconLabel);
        iconLabel->setToolTip(toolTips[forKind]);
        req->setToolTip(toolTips[forKind]);
        hLay->addWidget(req);
        ret->setLayout(hLay);
        lay->addWidget(ret);

        connect (req, &FileNameRequester::fileNameChanged, this,
                 [this, forKind](const QString &newName) {
                    mOutNames[forKind] = newName;
                 });
        return ret;
    }

public:
    void setOutputNames(const QMap<int, QString> &names) {
        Q_ASSERT(mOutNames.isEmpty());
        mOutNames = names;
        Q_FOREACH (int i, mOutNames.keys()) {
            mRequester[i] = createRequester(i, mOutLayout);
        }
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
            btn->setText(label);
            if (Kleo::gpgComplianceP("de-vs")) {
                bool de_vs = mWidget->isDeVsAndValid();
                btn->setIcon(QIcon::fromTheme(de_vs
                                             ? QStringLiteral("security-high")
                                             : QStringLiteral("security-medium")));
                btn->setStyleSheet(QStringLiteral("background-color: ") + (de_vs
                                   ? KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::PositiveBackground).color().name()
                                   : KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::NegativeBackground).color().name()));
                mParent->setLabelText(de_vs
                                      ? i18nc("VS-NfD-conforming is a German standard for restricted documents for which special restrictions about algorithms apply.  The string states that all cryptographic operations necessary for the communication are compliant with that.",
                                              "VS-NfD-compliant communication possible.")
                                      : i18nc("VS-NfD-conforming is a German standard for restricted documents for which special restrictions about algorithms apply.  The string states that all cryptographic operations necessary for the communication are compliant with that.",
                                              "VS-NfD-compliant communication not possible."));
            }
        } else {
            btn->setText(i18n("Next"));
            btn->setIcon(QIcon());
            btn->setStyleSheet(QString());
        }
        Q_EMIT completeChanged();
    }

    void updateFileWidgets()
    {
        if (mRequester.isEmpty()) {
            return;
        }
        const QVector<Key> recipients = mWidget->recipients();
        const Key sigKey = mWidget->signKey();
        bool pgp = mWidget->encryptSymmetric();
        bool cms = false;

        for (const Key &k : recipients) {
            if (pgp && cms) {
                break;
            }
            if (k.protocol() == Protocol::OpenPGP) {
                pgp = true;
            } else {
                cms = true;
            }
        }
        mOutLayout->setEnabled(false);
        mPlaceholderWidget->setVisible(!cms && !pgp && sigKey.isNull());
        mRequester[SignEncryptFilesWizard::SignatureCMS]->setVisible(!mUseOutputDir && sigKey.protocol() == Protocol::CMS);
        mRequester[SignEncryptFilesWizard::EncryptedCMS]->setVisible(!mUseOutputDir && cms);
        mRequester[SignEncryptFilesWizard::CombinedPGP]->setVisible(!mUseOutputDir && sigKey.protocol() == Protocol::OpenPGP && pgp);
        mRequester[SignEncryptFilesWizard::EncryptedPGP]->setVisible(!mUseOutputDir && pgp && sigKey.protocol() != Protocol::OpenPGP);
        mRequester[SignEncryptFilesWizard::SignaturePGP]->setVisible(!mUseOutputDir && sigKey.protocol() == Protocol::OpenPGP && !pgp);
        mRequester[SignEncryptFilesWizard::Directory]->setVisible(mUseOutputDir && !mPlaceholderWidget->isVisible());
        mOutLayout->setEnabled(true);
    }

private:
    SignEncryptFilesWizard *mParent;
    SignEncryptWidget *mWidget;
    QMap <int, QString> mOutNames;
    QMap <int, QWidget *> mRequester;
    QVBoxLayout *mOutLayout;
    QWidget *mPlaceholderWidget;
    QCheckBox *mUseOutputDirChk;
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
    , mSigningUserMutable(true)
    , mEncryptionUserMutable(true)
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
               (de_vs ? QWizard::HaveCustomButton1 : (QWizard::WizardOption) 0) |
               QWizard::NoBackButtonOnLastPage |
               QWizard::NoBackButtonOnStartPage);

    if (de_vs) {
        /* We use a custom button to display a label next to the
           buttons.  */
        mLabel = button(QWizard::CustomButton1);
        /* We style the button so that it looks and acts like a
           label.  */
        mLabel->setStyleSheet(QStringLiteral("border: none"));
        mLabel->setFocusPolicy(Qt::NoFocus);
    } else {
        mLabel = nullptr;
    }
}

void SignEncryptFilesWizard::setLabelText(const QString &label) const
{
    if (mLabel) {
        mLabel->setText(label);
    }
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

QVector<Key> SignEncryptFilesWizard::resolvedRecipients() const
{
    return mSigEncPage->recipients();
}

QVector<Key> SignEncryptFilesWizard::resolvedSigners() const
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
