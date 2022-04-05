/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/adduseriddialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "adduseriddialog.h"

#include <KLocalizedString>
#include <KSeparator>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include "kleopatra_debug.h"

using namespace Kleo;

namespace
{
QString buildUserId(const QString &name, const QString &email)
{
   if (name.isEmpty()) {
       return email;
   } else if (email.isEmpty()) {
       return name;
   } else {
       return QStringLiteral("%1 <%2>").arg(name, email);
   }
}
}

class AddUserIDDialog::Private
{
    friend class ::Kleo::AddUserIDDialog;
    AddUserIDDialog *const q;

    struct {
        QLineEdit *nameEdit;
        QLineEdit *emailEdit;
        QLabel *resultLabel;
        QDialogButtonBox *buttonBox;
    } ui;

public:
    explicit Private(AddUserIDDialog *qq)
        : q{qq}
    {
        q->setWindowTitle(i18nc("title:window", "Add User ID"));

        auto mainLayout = new QVBoxLayout{q};

        mainLayout->addWidget(new QLabel{i18n("Enter the name and/or the email address to use for the user ID."), q});

        auto gridLayout = new QGridLayout;
        int row = -1;

        {
            auto label = new QLabel{i18nc("@label", "Name:"), q};
            ui.nameEdit = new QLineEdit{q};
            label->setBuddy(ui.nameEdit);

            row++;
            gridLayout->addWidget(label, row, 0, 1, 1);
            gridLayout->addWidget(ui.nameEdit, row, 1, 1, 1);
        }
        connect(ui.nameEdit, &QLineEdit::textChanged,
                q, [this]() { onNameChanged(); });

        {
            auto label = new QLabel{i18nc("@label", "Email:"), q};
            ui.emailEdit = new QLineEdit{q};
            label->setBuddy(ui.emailEdit);

            row++;
            gridLayout->addWidget(label, row, 0, 1, 1);
            gridLayout->addWidget(ui.emailEdit, row, 1, 1, 1);
        }
        connect(ui.emailEdit, &QLineEdit::textChanged,
                q, [this]() { onEmailChanged(); });

        mainLayout->addLayout(gridLayout);

        mainLayout->addWidget(new KSeparator{Qt::Horizontal, q});

        {
            auto label = new QLabel{i18n("This is how the new user ID will be stored in the certificate:"), q};
            mainLayout->addWidget(label);
        }
        {
            ui.resultLabel = new QLabel{q};
            ui.resultLabel->setMinimumSize(300, 0);
            ui.resultLabel->setAlignment(Qt::AlignCenter);
            ui.resultLabel->setTextFormat(Qt::PlainText);
            QFont font;
            font.setBold(true);
            font.setWeight(75);
            ui.resultLabel->setFont(font);

            mainLayout->addWidget(ui.resultLabel);
        }

        mainLayout->addWidget(new KSeparator{Qt::Horizontal, q});

        mainLayout->addStretch(1);

        ui.buttonBox = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, q};

        mainLayout->addWidget(ui.buttonBox);

        connect(ui.buttonBox, &QDialogButtonBox::accepted, q, &QDialog::accept);
        connect(ui.buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);

        updateResultLabel();
    }

    QString name() const
    {
        return ui.nameEdit->text().trimmed();
    }

    QString email() const
    {
        return ui.emailEdit->text().trimmed();
    }

private:
    void onNameChanged()
    {
        updateResultLabel();
    }

    void onEmailChanged()
    {
        updateResultLabel();
    }

    void updateResultLabel()
    {
        ui.resultLabel->setText(buildUserId(name(), email()));
    }
};

AddUserIDDialog::AddUserIDDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog{parent, f}
    , d(new Private{this})
{
}

AddUserIDDialog::~AddUserIDDialog() = default;

void AddUserIDDialog::setName(const QString &name)
{
    d->ui.nameEdit->setText(name);
}

QString AddUserIDDialog::name() const
{
    return d->name();
}

void AddUserIDDialog::setEmail(const QString &email)
{
    d->ui.emailEdit->setText(email);
}

QString AddUserIDDialog::email() const
{
    return d->email();
}
