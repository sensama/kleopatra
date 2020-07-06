/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/addemaildialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2019 g10 Code GmbH

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

#include "addemaildialog.h"

#include <utils/validation.h>

#include <QString>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <KLocalizedString>
#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Dialogs;

class AddEmailDialog::Private
{
public:
    Private(AddEmailDialog *qq):
        q(qq),
        mAdvancedSelected(false)
    {
        auto mainLay = new QVBoxLayout(q);

        auto btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        mOkButton = btnBox->button(QDialogButtonBox::Ok);
        QObject::connect (btnBox, &QDialogButtonBox::accepted, q, [this] () {
                q->accept();
            });
        QObject::connect (btnBox, &QDialogButtonBox::rejected, q, &QDialog::reject);

        btnBox->addButton(i18n("Advanced"), QDialogButtonBox::HelpRole);
        QObject::connect (btnBox, &QDialogButtonBox::helpRequested, q, [this] () {
                mAdvancedSelected = true;
                q->accept();
            });


        mainLay->addStretch(-1);

        auto emailLay = new QHBoxLayout;
        auto emailLbl = new QLabel(i18n("EMail") + QLatin1Char(':'));
        mEmailEdit = new QLineEdit(q);
        mEmailEdit->setValidator(Validation::email(mEmailEdit));

        connect(mEmailEdit, &QLineEdit::textChanged, q, [this] () {
                mOkButton->setEnabled(!mEmailEdit->text().isEmpty() && mEmailEdit->hasAcceptableInput());
            });

        emailLbl->setBuddy(mEmailEdit);

        emailLay->addWidget(emailLbl);
        emailLay->addWidget(mEmailEdit);

        mainLay->addLayout(emailLay);
        mainLay->addWidget(btnBox);
        mOkButton->setEnabled(!mEmailEdit->text().isEmpty() && mEmailEdit->hasAcceptableInput());
    }

    AddEmailDialog *const q;
    QPushButton *mOkButton;
    QLineEdit *mEmailEdit;
    bool mAdvancedSelected;
};

AddEmailDialog::AddEmailDialog(QWidget *parent):
    QDialog(parent),
    d(new Private(this))
{
    setWindowTitle(i18nc("@title:window", "Add New EMail"));
}

AddEmailDialog::~AddEmailDialog()
{
}

void AddEmailDialog::setEmail(const QString &email)
{
    return d->mEmailEdit->setText(email);
}

QString AddEmailDialog::email() const
{
    return d->mEmailEdit->text().trimmed();
}

bool AddEmailDialog::advancedSelected()
{
    return d->mAdvancedSelected;
}
