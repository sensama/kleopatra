/* -*- mode: c++; c-basic-offset:4 -*-
    padwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2018 Intevation GmbH

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
#include "padwidget.h"

#include "kleopatra_debug.h"

#include <Libkleo/Exception>
#include <Libkleo/Classify>

#include "crypto/gui/signencryptwidget.h"
#include "crypto/gui/resultitemwidget.h"

#include "crypto/signencrypttask.h"
#include "crypto/decryptverifytask.h"
#include "utils/gnupg-helper.h"
#include "utils/input.h"
#include "utils/output.h"

#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGroupBox>
#include <QPushButton>
#include <QProgressBar>
#include <QSplitter>
#include <QLabel>

#include <KLocalizedString>
#include <KColorScheme>
#include <KMessageBox>
#include <KSplitterCollapserButton>

using namespace Kleo;
using namespace Kleo::Crypto;
using namespace Kleo::Crypto::Gui;

class PadWidget::Private
{
public:
    Private(PadWidget *qq):
        q(qq),
        mEdit(new QTextEdit),
        mCryptBtn(new QPushButton(QIcon::fromTheme("document-edit-sign-encrypt"), i18n("Sign / Encrypt Notepad"))),
        mDecryptBtn(new QPushButton(QIcon::fromTheme("document-edit-decrypt-verify"), i18n("Decrypt / Verify Notepad"))),
        mRevertBtn(new QPushButton(QIcon::fromTheme("edit-undo"), i18n("Revert"))),
        mAdditionalInfoLabel(new QLabel),
        mSigEncWidget(new SignEncryptWidget),
        mProgressBar(new QProgressBar),
        mProgressLabel(new QLabel),
        mLastResultWidget(nullptr)
    {
        auto vLay = new QVBoxLayout(q);

        auto btnLay = new QHBoxLayout;
        vLay->addLayout(btnLay);
        btnLay->addWidget(mCryptBtn);
        btnLay->addWidget(mDecryptBtn);
        btnLay->addWidget(mRevertBtn);

        mRevertBtn->setVisible(false);

        btnLay->addWidget(mAdditionalInfoLabel);

        btnLay->addStretch(-1);

        mProgressBar->setRange(0, 0);
        mProgressBar->setVisible(false);
        mProgressLabel->setVisible(false);
        auto progLay = new QHBoxLayout;

        progLay->addWidget(mProgressLabel);
        progLay->addWidget(mProgressBar);

        mStatusLay = new QVBoxLayout;
        mStatusLay->addLayout(progLay);
        vLay->addLayout(mStatusLay, 0.1);

        auto splitter = new QSplitter;
        vLay->addWidget(splitter, 1);

        splitter->addWidget(mEdit);

        // The recipients area
        auto recipientsWidget = new QWidget;
        auto recipientsVLay = new QVBoxLayout(recipientsWidget);
        auto protocolSelectionLay = new QHBoxLayout;
        recipientsVLay->addLayout(protocolSelectionLay);
        protocolSelectionLay->addWidget(new QLabel(i18n("<h3>Recipients:</h3>")));
        protocolSelectionLay->addStretch(-1);
        // Once S/MIME is supported add radio for S/MIME here.

        recipientsVLay->addWidget(mSigEncWidget);
        splitter->addWidget(recipientsWidget);
        new KSplitterCollapserButton(recipientsWidget, splitter);

        mEdit->setPlaceholderText("Enter a message to encrypt or decrypt...");

        auto fixedFont = QFont("Monospace", 10);
        // This does not work well:
        // QFontDatabase::systemFont(QFontDatabase::FixedFont);

        mEdit->setCurrentFont(fixedFont);
        mEdit->setMinimumWidth(QFontMetrics(fixedFont).averageCharWidth() * 70);

        mSigEncWidget->setProtocol(GpgME::OpenPGP);

        updateCommitButton();

        connect(mCryptBtn, &QPushButton::clicked, q, [this] () {
                doEncryptSign();
            });

        connect(mSigEncWidget, &SignEncryptWidget::operationChanged, q, [this] (const QString &) {
                updateCommitButton();
            });

        connect(mDecryptBtn, &QPushButton::clicked, q, [this] () {
                doDecryptVerify();
            });

        connect(mRevertBtn, &QPushButton::clicked, q, [this] () {
                revert();
            });
    }

    void revert()
    {
        mEdit->setPlainText(QString::fromUtf8(mInputData));
        mRevertBtn->setVisible(false);
    }

    void cryptDone(const std::shared_ptr<const Kleo::Crypto::Task::Result> &result)
    {
        updateCommitButton();
        mDecryptBtn->setEnabled(true);
        mProgressBar->setVisible(false);
        mProgressLabel->setVisible(false);

        mLastResultWidget = new ResultItemWidget(result);
        mLastResultWidget->showCloseButton(true);
        mStatusLay->addWidget(mLastResultWidget);

        connect(mLastResultWidget, &ResultItemWidget::closeButtonClicked, q, [this] () {
            removeLastResultItem();
        });

        if (result->errorCode()) {
            if (!result->errorString().isEmpty()) {
                KMessageBox::error(q,
                        result->errorString(),
                        i18nc("@title", "Error in crypto action"));
            }
            return;
        }
        mEdit->setPlainText(QString::fromUtf8(mOutputData));
        mOutputData.clear();
        mRevertBtn->setVisible(true);
    }

    void doDecryptVerify()
    {
        doCryptoCommon();
        mProgressLabel->setText(i18n("Decrypt / Verify") + QStringLiteral("..."));
        auto input = Input::createFromByteArray(&mInputData,  i18n("Notepad"));
        auto output = Output::createFromByteArray(&mOutputData, i18n("Notepad"));

        AbstractDecryptVerifyTask *task;
        auto classification = input->classification();
        if (classification & Class::OpaqueSignature ||
            classification & Class::ClearsignedMessage) {
            auto verifyTask = new VerifyOpaqueTask();
            verifyTask->setInput(input);
            verifyTask->setOutput(output);
            task = verifyTask;
        } else {
            auto decTask = new DecryptVerifyTask();
            decTask->setInput(input);
            decTask->setOutput(output);
            task = decTask;
        }
        try {
            task->autodetectProtocolFromInput();
        } catch (const Kleo::Exception &e) {
            KMessageBox::error(q,
                    e.message(),
                    i18nc("@title", "Error in crypto action"));
            mCryptBtn->setEnabled(true);
            mDecryptBtn->setEnabled(true);
            mProgressBar->setVisible(false);
            mProgressLabel->setVisible(false);
            return;
        }

        connect (task, &Task::result, q, [this, task] (const std::shared_ptr<const Kleo::Crypto::Task::Result> &result) {
                qCDebug(KLEOPATRA_LOG) << "Decrypt / Verify done. Err:" << result->errorCode();
                task->deleteLater();
                cryptDone(result);
            });
        task->start();
    }

    void removeLastResultItem()
    {
        if (mLastResultWidget) {
            mStatusLay->removeWidget(mLastResultWidget);
            delete mLastResultWidget;
            mLastResultWidget = nullptr;
        }
    }

    void doCryptoCommon()
    {
        mCryptBtn->setEnabled(false);
        mDecryptBtn->setEnabled(false);
        mProgressBar->setVisible(true);
        mProgressLabel->setVisible(true);
        mInputData = mEdit->toPlainText().toUtf8();
        removeLastResultItem();
    }

    void doEncryptSign()
    {
        doCryptoCommon();
        mProgressLabel->setText(mSigEncWidget->currentOp() + QStringLiteral("..."));
        auto input = Input::createFromByteArray(&mInputData,  i18n("Notepad"));
        auto output = Output::createFromByteArray(&mOutputData, i18n("Notepad"));

        auto task = new SignEncryptTask();
        task->setInput(input);
        task->setOutput(output);

        const auto sigKey = mSigEncWidget->signKey();
        if (!sigKey.isNull()) {
            task->setSign(true);
            std::vector<GpgME::Key> signVector;
            signVector.push_back(sigKey);
            task->setSigners(signVector);
        } else {
            task->setSign(false);
        }
        task->setEncrypt(mSigEncWidget->encryptSymmetric() || mSigEncWidget->recipients().size());
        task->setRecipients(mSigEncWidget->recipients().toStdVector());
        task->setEncryptSymmetric(mSigEncWidget->encryptSymmetric());
        task->setAsciiArmor(true);

        connect (task, &Task::result, q, [this, task] (const std::shared_ptr<const Kleo::Crypto::Task::Result> &result) {
                qCDebug(KLEOPATRA_LOG) << "Encrypt / Sign done. Err:" << result->errorCode();
                task->deleteLater();
                cryptDone(result);
            });
        task->start();
    }

    void updateCommitButton()
    {
        mAdditionalInfoLabel->setVisible(false);
        if (!mSigEncWidget->currentOp().isEmpty()) {
            mCryptBtn->setDisabled(false);
            mCryptBtn->setText(mSigEncWidget->currentOp() + QLatin1Char(' ') + i18n("Notepad"));
        } else {
            mCryptBtn->setText(i18n("Sign / Encrypt Notepad"));
            mCryptBtn->setDisabled(true);
        }

        if (Kleo::gpgComplianceP("de-vs")) {
            bool de_vs = mSigEncWidget->isDeVsAndValid();
            mCryptBtn->setIcon(QIcon::fromTheme(de_vs
                        ? "security-high"
                        : "security-medium"));
            mCryptBtn->setStyleSheet(QStringLiteral("background-color: ") + (de_vs
                        ? KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::PositiveBackground).color().name()
                        : KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::NegativeBackground).color().name()));
            mAdditionalInfoLabel->setText(de_vs
                    ? i18nc("VS-NfD-conforming is a German standard for restricted documents for which special restrictions about algorithms apply.  The string states that all cryptographic operations necessary for the communication are compliant with that.",
                        "VS-NfD-compliant communication possible.")
                    : i18nc("VS-NfD-conforming is a German standard for restricted documents for which special restrictions about algorithms apply.  The string states that all cryptographic operations necessary for the communication are compliant with that.",
                        "VS-NfD-compliant communication not possible."));
            mAdditionalInfoLabel->setVisible(true);
        }
    }

private:
    PadWidget *q;
    QTextEdit *mEdit;
    QPushButton *mCryptBtn;
    QPushButton *mDecryptBtn;
    QPushButton *mRevertBtn;
    QLabel *mAdditionalInfoLabel;
    QByteArray mInputData;
    QByteArray mOutputData;
    SignEncryptWidget *mSigEncWidget;
    QProgressBar *mProgressBar;
    QLabel *mProgressLabel;
    QVBoxLayout *mStatusLay;
    ResultItemWidget *mLastResultWidget;
};

PadWidget::PadWidget(QWidget *parent):
    QWidget(parent),
    d(new Private(this))
{
}
