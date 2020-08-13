/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/addemaildialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019 g 10 Code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
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
