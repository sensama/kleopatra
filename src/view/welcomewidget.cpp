/*  view/welcomewidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "welcomewidget.h"

#include "htmllabel.h"

#include <version-kleopatra.h>
#include <Libkleo/GnuPG>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QToolButton>
#include <QAction>

#include "commands/importcertificatefromfilecommand.h"
#include "commands/newopenpgpcertificatecommand.h"

#include <KLocalizedString>
#include <KSharedConfig>
#include <KConfigGroup>

static const QString templ = QStringLiteral(
"<h3>%1</h3>" // Welcome
"<p>%2<p/><p>%3</p>" // Intro + Explanation
"<ul><li>%4</li><li>%5</li></ul>" //
"<p>%6</p>" // More info
"");

using namespace Kleo;

namespace
{
/**
 * A tool button that can be activated with the Enter and Return keys additionally to the Space key.
 */
class ToolButton : public QToolButton
{
    Q_OBJECT
public:
    using QToolButton::QToolButton;

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        switch (e->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return: {
            // forward as key press of Key_Select to QToolButton
            QKeyEvent alternateEvent{e->type(), Qt::Key_Select, e->modifiers(), e->nativeScanCode(), e->nativeVirtualKey(), e->nativeModifiers(), e->text(), e->isAutoRepeat(), static_cast<ushort>(e->count())};
            QToolButton::keyPressEvent(&alternateEvent);
            if (!alternateEvent.isAccepted()) {
                e->ignore();
            }
            break;
        }
        default:
            QToolButton::keyPressEvent(e);
        }
    }

    void keyReleaseEvent(QKeyEvent *e) override
    {
        switch (e->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return: {
            // forward as key release of Key_Select to QToolButton
            QKeyEvent alternateEvent{e->type(), Qt::Key_Select, e->modifiers(), e->nativeScanCode(), e->nativeVirtualKey(), e->nativeModifiers(), e->text(), e->isAutoRepeat(), static_cast<ushort>(e->count())};
            QToolButton::keyReleaseEvent(&alternateEvent);
            if (!alternateEvent.isAccepted()) {
                e->ignore();
            }
            break;
        }
        default:
            QToolButton::keyReleaseEvent(e);
        }
    }
};
}

class WelcomeWidget::Private
{
public:
    Private(WelcomeWidget *qq): q(qq)
    {
        auto vLay = new QVBoxLayout(q);
        auto hLay = new QHBoxLayout;

        const QString welcome = i18nc("%1 is version", "Welcome to Kleopatra %1",
#ifdef Q_OS_WIN
                                      Kleo::gpg4winVersion());
#else
                                      QStringLiteral(KLEOPATRA_VERSION_STRING));
#endif
        const QString introduction = i18n("Kleopatra is a front-end for the crypto software <a href=\"https://gnupg.org\">GnuPG</a>.");

        const QString keyExplanation = i18n("For most actions you need either a public key (certificate) or your own private key.");

        const QString privateKeyExplanation = i18n("The private key is needed to decrypt or sign.");
        const QString publicKeyExplanation = i18n("The public key can be used by others to verify your identity or encrypt to you.");

        const QString wikiUrl = i18nc("More info about public key cryptography, please link to your local version of Wikipedia",
                                      "https://en.wikipedia.org/wiki/Public-key_cryptography");
        const QString learnMore = i18nc("%1 is link a wiki article", "You can learn more about this on <a href=\"%1\">Wikipedia</a>.", wikiUrl);

        const auto labelText = templ.arg(welcome).arg(introduction).arg(keyExplanation).arg(privateKeyExplanation).arg(publicKeyExplanation).arg(learnMore);
        mLabel = new HtmlLabel{labelText, q};
        mLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
        mLabel->setOpenExternalLinks(true);

        auto genKeyAction = new QAction(q);
        genKeyAction->setText(i18n("New Key Pair..."));
        genKeyAction->setIcon(QIcon::fromTheme(QStringLiteral("view-certificate-add")));

        auto importAction = new QAction(q);
        importAction->setText(i18n("Import..."));
        importAction->setIcon(QIcon::fromTheme(QStringLiteral("view-certificate-import")));

        connect(importAction, &QAction::triggered, q, [this] () { import(); });
        connect(genKeyAction, &QAction::triggered, q, [this] () { generate(); });

        mGenerateBtn = new ToolButton{q};
        mGenerateBtn->setDefaultAction(genKeyAction);
        mGenerateBtn->setIconSize(QSize(64, 64));
        mGenerateBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        const auto generateBtnDescription = kxi18nc("@info",
                                                    "Create a new OpenPGP key pair.<nl/>"
                                                    "To create an S/MIME certificate request use "
                                                    "<interface>New S/MIME Certification Request</interface> "
                                                    "from the <interface>File</interface> menu instead.");
        mGenerateBtn->setToolTip(generateBtnDescription.toString());
        mGenerateBtn->setAccessibleDescription(generateBtnDescription.toString(Kuit::PlainText));

        KConfigGroup restrictions(KSharedConfig::openConfig(), "KDE Action Restrictions");
        mGenerateBtn->setEnabled(restrictions.readEntry("action/file_new_certificate", true));

        mImportBtn = new ToolButton{q};
        mImportBtn->setDefaultAction(importAction);
        mImportBtn->setIconSize(QSize(64, 64));
        mImportBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        const auto importBtnDescription = kxi18nc("@info",
                                                  "Import certificate from a file.<nl/>"
                                                  "To import from a public keyserver use <interface>Lookup on Server</interface> instead.");
        mImportBtn->setToolTip(importBtnDescription.toString());
        mImportBtn->setAccessibleDescription(importBtnDescription.toString(Kuit::PlainText));
        mImportBtn->setEnabled(restrictions.readEntry("action/file_import_certificate", true));

        auto btnLayout = new QHBoxLayout;
        btnLayout->addStretch(-1);
        btnLayout->addWidget(mGenerateBtn);
        btnLayout->addWidget(mImportBtn);
        btnLayout->addStretch(-1);

        vLay->addStretch(-1);
        vLay->addLayout(hLay);
        vLay->addLayout(btnLayout);
        vLay->addStretch(-1);

        hLay->addStretch(-1);
        hLay->addWidget(mLabel);
        hLay->addStretch(-1);
    }

    void import()
    {
        mImportBtn->setEnabled(false);
        auto cmd = new Kleo::ImportCertificateFromFileCommand();
        cmd->setParentWidget(q);

        QObject::connect(cmd, &Kleo::ImportCertificateFromFileCommand::finished,
                q, [this]() {
            mImportBtn->setEnabled(true);
        });
        cmd->start();
    }

    void generate()
    {
        mGenerateBtn->setEnabled(false);
        auto cmd = new NewOpenPGPCertificateCommand;
        cmd->setParentWidget(q);

        QObject::connect(cmd, &NewOpenPGPCertificateCommand::finished,
                q, [this]() {
            mGenerateBtn->setEnabled(true);
        });
        cmd->start();
    }

    WelcomeWidget *const q;
    HtmlLabel *mLabel = nullptr;
    ToolButton *mGenerateBtn = nullptr;
    ToolButton *mImportBtn = nullptr;
};


WelcomeWidget::WelcomeWidget (QWidget *parent): QWidget(parent),
    d(new Private(this))
{
}

void WelcomeWidget::focusFirstChild(Qt::FocusReason reason)
{
    d->mLabel->setFocus(reason);
}

#include "welcomewidget.moc"

#include "moc_welcomewidget.cpp"
