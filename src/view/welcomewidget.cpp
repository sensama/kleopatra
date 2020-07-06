/*  view/smartcardwidget.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2017 by Bundesamt für Sicherheit in der Informationstechnik
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

#include "welcomewidget.h"

#include <version-kleopatra.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QAction>

#include "commands/importcertificatefromfilecommand.h"
#include "commands/newcertificatecommand.h"

#include <KLocalizedString>

static const QString templ = QStringLiteral(
"<h3>%1</h3>" // Welcome
"<p>%2<p/><p>%3</p>" // Intro + Explanation
"<ul><li>%4</li><li>%5</li></ul>" //
"<p>%6</p>" // More info
"");

using namespace Kleo;

class WelcomeWidget::Private
{
public:
    Private(WelcomeWidget *qq): q(qq)
    {
        auto vLay = new QVBoxLayout(q);
        auto hLay = new QHBoxLayout;

        const QString welcome = i18nc("%1 is version", "Welcome to Kleopatra %1",
                                      QStringLiteral(KLEOPATRA_VERSION_STRING));
        const QString introduction = i18n("Kleopatra is a front-end for the crypto software <a href=\"https://gnupg.org\">GnuPG</a>.");

        const QString keyExplanation = i18n("For most actions you need either a public key (certificate) or your own private key.");

        const QString privateKeyExplanation = i18n("The private key is needed to decrypt or sign.");
        const QString publicKeyExplanation = i18n("The public key can be used by others to verify your identity or encrypt to you.");

        const QString wikiUrl = i18nc("More info about public key cryptography, please link to your local version of Wikipedia",
                                      "https://en.wikipedia.org/wiki/Public-key_cryptography");
        const QString learnMore = i18nc("%1 is link a wiki article", "You can learn more about this on <a href=\"%1\">Wikipedia</a>.", wikiUrl);

        auto label = new QLabel(templ.arg(welcome).arg(introduction).arg(keyExplanation).arg(privateKeyExplanation).arg(publicKeyExplanation).arg(learnMore));
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        label->setOpenExternalLinks(true);

        auto genKeyAction = new QAction(q);
        genKeyAction->setText(i18n("New Key Pair..."));
        genKeyAction->setIcon(QIcon::fromTheme(QStringLiteral("view-certificate-add")));

        auto importAction = new QAction(q);
        importAction->setText(i18n("Import..."));
        importAction->setIcon(QIcon::fromTheme(QStringLiteral("view-certificate-import")));

        connect(importAction, &QAction::triggered, q, [this] () { import(); });
        connect(genKeyAction, &QAction::triggered, q, [this] () { generate(); });

        mGenerateBtn = new QToolButton();
        mGenerateBtn->setDefaultAction(genKeyAction);
        mGenerateBtn->setIconSize(QSize(64, 64));
        mGenerateBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        mGenerateBtn->setToolTip(i18n("Create a new OpenPGP key pair") + QStringLiteral("<br>") +
                                 i18n("To create an S/MIME certificate request use \"New Key Pair\" from the 'File' Menu instead"));

        mImportBtn = new QToolButton();
        mImportBtn->setDefaultAction(importAction);
        mImportBtn->setIconSize(QSize(64, 64));
        mImportBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        mImportBtn->setToolTip(i18n("Import from a file.") + QStringLiteral("<br>") +
                               i18n("To import from a public keyserver use \"Lookup on Server\" instead."));

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
        hLay->addWidget(label);
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
        auto cmd = new Commands::NewCertificateCommand();
        cmd->setProtocol(GpgME::OpenPGP);
        cmd->setParentWidget(q);

        QObject::connect(cmd, &Commands::NewCertificateCommand::finished,
                q, [this]() {
            mGenerateBtn->setEnabled(true);
        });
        cmd->start();
    }

    WelcomeWidget *const q;
    QToolButton *mGenerateBtn;
    QToolButton *mImportBtn;
};


WelcomeWidget::WelcomeWidget (QWidget *parent): QWidget(parent),
    d(new Private(this))
{
}
