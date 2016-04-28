/*  crypto/gui/signencryptfileswizard.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2009 Klarälvdalens Datakonsult AB
                  2016 Intevation GmbH

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

#include <KMessageBox>

#include "kleopatra_debug.h"
#include <QVBoxLayout>
#include <QWizardPage>

#include <QPointer>

#include <gpgme++/key.h>

using namespace GpgME;
using namespace Kleo;
using namespace Kleo::Crypto::Gui;

enum Page {
    SigEncPageId,
    ResultPageId,

    NumPages
};

/*
FIXME: Either show this warning or fix detached signatures for archives.
    bool validatePage() Q_DECL_OVERRIDE {
        if (isSignOnlySelected() && isArchiveRequested())
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
        else {
            return true;
        }
    }
*/

class SigEncPage: public QWizardPage
{
    Q_OBJECT
public:
    explicit SigEncPage(QWidget *parent = Q_NULLPTR)
        : QWizardPage(parent),
          mWidget(new SignEncryptWidget)
    {
        setTitle(i18nc("@title", "Sign / Encrypt Files"));
        QVBoxLayout *vLay = new QVBoxLayout(this);
        vLay->addWidget(mWidget);
        connect(mWidget, &SignEncryptWidget::operationChanged, this,
                &SigEncPage::updateCommitButton);
        setLayout(vLay);
        updateCommitButton(mWidget->currentOp());
    }

    bool isComplete() const Q_DECL_OVERRIDE
    {
        return !mWidget->currentOp().isNull();
    }

    int nextId() const Q_DECL_OVERRIDE
    {
        return ResultPageId;
    }

    void initializePage() Q_DECL_OVERRIDE {
        setCommitPage(true);
    }

    bool validatePage() Q_DECL_OVERRIDE {
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
        if (!hasSecret)
        {
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

private Q_SLOTS:
    void updateCommitButton(const QString &label)
    {
        if (!label.isEmpty()) {
            setButtonText(QWizard::CommitButton, label);
        } else {
            setButtonText(QWizard::CommitButton, i18n("Next"));
        }
        Q_EMIT completeChanged();
    }

private:
    SignEncryptWidget *mWidget;
};

class ResultPage : public NewResultPage
{
    Q_OBJECT
public:
    explicit ResultPage(QWidget *parent = Q_NULLPTR)
        : NewResultPage(parent)
    {
        setTitle(i18nc("@title", "Results"));
        setSubTitle(i18nc("@title",
                          "Status and progress of the crypto operations is shown here."));
    }

};

SignEncryptFilesWizard::SignEncryptFilesWizard(QWidget *parent, Qt::WindowFlags f)
    : QWizard(parent, f)
{
    mSigEncPage = new SigEncPage(this);
    mResultPage = new ResultPage(this);

    connect(this, &QWizard::currentIdChanged, this,
            &SignEncryptFilesWizard::slotCurrentIdChanged);
    setPage(SigEncPageId, mSigEncPage);
    setPage(ResultPageId, mResultPage);
    setOptions(QWizard::IndependentPages |
               QWizard::NoBackButtonOnLastPage |
               QWizard::NoBackButtonOnStartPage);
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
}

void SignEncryptFilesWizard::setSigningPreset(bool preset)
{
    if (preset == mSigningPreset) {
        return;
    }
    mSigningPreset = preset;
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
    if (preset == mEncryptionPreset) {
        return;
    }
    mEncryptionPreset = preset;
}

void SignEncryptFilesWizard::setEncryptionUserMutable(bool mut)
{
    if (mut == mEncryptionUserMutable) {
        return;
    }
    mEncryptionUserMutable = mut;
}

QVector<Key> SignEncryptFilesWizard::resolvedRecipients() const
{
    return mSigEncPage->recipients();
}

QVector<Key> SignEncryptFilesWizard::resolvedSigners() const
{
    return mSigEncPage->signers();
}

void SignEncryptFilesWizard::setTaskCollection(const boost::shared_ptr<Kleo::Crypto::TaskCollection> &coll)
{
    mResultPage->setTaskCollection(coll);
}

#include "signencryptfileswizard.moc"
