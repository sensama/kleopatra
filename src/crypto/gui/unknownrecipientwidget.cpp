/*  crypto/gui/unknownrecipientwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2018 by Intevation GmbH

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

#include "unknownrecipientwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>

#include "commands/lookupcertificatescommand.h"

#include <KLocalizedString>

using namespace Kleo;
UnknownRecipientWidget::UnknownRecipientWidget(const char *keyid,
                                               QWidget *parent):
    QWidget(parent)
{
    auto hLay = new QHBoxLayout(this);

    auto caption = new QLabel(i18nc("Caption for an unknown key/certificate where only ID is known.",
                              "Unknown Recipient:"));

    mKeyID = QString::fromLatin1(keyid);

    auto keyIdLabel = new QLabel(mKeyID);
    keyIdLabel->setFont(QFont(QStringLiteral("Monospace")));

    auto lookUpBtn = new QPushButton(i18n("Search"));


    lookUpBtn->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    lookUpBtn->setToolTip(i18n("Search on keyserver"));
    connect (lookUpBtn, &QPushButton::clicked, this, [this, lookUpBtn] () {
        lookUpBtn->setEnabled(false);
        auto cmd = new Kleo::Commands::LookupCertificatesCommand(mKeyID, nullptr);
        connect(cmd, &Kleo::Commands::LookupCertificatesCommand::finished,
                this, [lookUpBtn]() {
                    lookUpBtn->setEnabled(true);
                });
        cmd->setParentWidget(this->parentWidget());
        cmd->start();
    });

    hLay->addWidget(caption);
    hLay->addWidget(keyIdLabel);
    hLay->addWidget(lookUpBtn);
    hLay->addStretch(1);

    setToolTip(i18n("The data was encrypted to this key / certificate."));
}

QString UnknownRecipientWidget::keyID() const {
    return mKeyID;
}
