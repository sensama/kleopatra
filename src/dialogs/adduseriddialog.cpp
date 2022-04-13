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

#include "utils/accessibility.h"
#include "view/errorlabel.h"
#include "view/formtextinput.h"
#include "view/htmllabel.h"

#include <utils/validation.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSeparator>
#include <KSharedConfig>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QValidator>
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
        std::unique_ptr<FormTextInput<QLineEdit>> nameInput;
        std::unique_ptr<FormTextInput<QLineEdit>> emailInput;
        HtmlLabel *resultLabel;
        QDialogButtonBox *buttonBox;
    } ui;

    LabelHelper labelHelper;

public:
    explicit Private(AddUserIDDialog *qq)
        : q{qq}
    {
        q->setWindowTitle(i18nc("title:window", "Add User ID"));

        const KConfigGroup config{KSharedConfig::openConfig(), "CertificateCreationWizard"};
        const auto attrOrder = config.readEntry("OpenPGPAttributeOrder", QStringList{});
        const auto nameIsRequired = attrOrder.contains(QLatin1String{"NAME!"}, Qt::CaseInsensitive);
        const auto emailIsRequired = attrOrder.contains(QLatin1String{"EMAIL!"}, Qt::CaseInsensitive);

        auto mainLayout = new QVBoxLayout{q};

        const auto infoText = nameIsRequired || emailIsRequired
            ? i18n("Enter a name and an email address to use for the user ID.")
            : i18n("Enter a name and/or an email address to use for the user ID.");
        mainLayout->addWidget(new QLabel{infoText, q});

        auto gridLayout = new QGridLayout;
        int row = -1;

        {
            ui.nameInput = FormTextInput<QLineEdit>::create(q);
            ui.nameInput->setLabelText(i18nc("@label", "Name"));
            ui.nameInput->setIsRequired(nameIsRequired);
            ui.nameInput->setValueRequiredErrorMessage(i18n("Error: The name is required."));
            const auto regexp = config.readEntry("NAME_regex");
            if (regexp.isEmpty()) {
                ui.nameInput->setValidator(Validation::simpleName(Validation::Optional, q));
                ui.nameInput->setToolTip(xi18n(
                    "<para>The name must not contain any of the following characters: &lt;, &gt;, @.</para>"));
                ui.nameInput->setAccessibleDescription(i18nc("text for screen readers",
                    "The name must not contain any of the following characters: less-than sign, greater-than sign, at sign."));
                ui.nameInput->setInvalidEntryErrorMessage(i18n("Error: The entered name contains invalid characters."));
            } else {
                ui.nameInput->setValidator(Validation::simpleName(regexp, Validation::Optional, q));
                ui.nameInput->setToolTip(xi18n(
                    "<para>The name must not contain any of the following characters: &lt;, &gt;, @. "
                    "Additionally, the name must follow the rules set by your organization.</para>"));
                ui.nameInput->setAccessibleDescription(i18nc("text for screen readers",
                    "The name must not contain any of the following characters: less-than sign, greater-than sign, at sign. "
                    "Additionally, the name must follow the rules set by your organization."));
                ui.nameInput->setInvalidEntryErrorMessage(i18n(
                    "Error: The entered name contains invalid characters "
                    "or it does not follow your organization's rules."));
            }

            row++;
            gridLayout->addWidget(ui.nameInput->label(), row, 0, 1, 1);
            gridLayout->addWidget(ui.nameInput->widget(), row, 1, 1, 1);
            row++;
            gridLayout->addWidget(ui.nameInput->errorLabel(), row, 0, 1, 2);
        }
        connect(ui.nameInput->widget(), &QLineEdit::textChanged,
                q, [this]() { updateResultLabel(); });

        {
            ui.emailInput = FormTextInput<QLineEdit>::create(q);
            ui.emailInput->setLabelText(i18nc("@label", "Email address"));
            ui.emailInput->setIsRequired(emailIsRequired);
            ui.emailInput->setValueRequiredErrorMessage(i18n("Error: The email address is required."));
            const auto regexp = config.readEntry(QLatin1String("EMAIL_regex"));
            if (regexp.isEmpty()) {
                ui.emailInput->setValidator(Validation::email(Validation::Optional, q));
                ui.emailInput->setInvalidEntryErrorMessage(i18n("Error: The entered email address is not valid."));
            } else {
                ui.emailInput->setValidator(Validation::email(regexp, Validation::Optional, q));
                ui.emailInput->setToolTip(xi18n(
                    "<para>If an email address is given, then it has to satisfy the rules set by your organization.</para>"));
                ui.emailInput->setInvalidEntryErrorMessage(i18n(
                    "Error: The entered email address is not valid "
                    "or it does not follow your organization's rules."));
            }

            row++;
            gridLayout->addWidget(ui.emailInput->label(), row, 0, 1, 1);
            gridLayout->addWidget(ui.emailInput->widget(), row, 1, 1, 1);
            row++;
            gridLayout->addWidget(ui.emailInput->errorLabel(), row, 0, 1, 2);
        }
        connect(ui.emailInput->widget(), &QLineEdit::textChanged,
                q, [this]() { updateResultLabel(); });

        mainLayout->addLayout(gridLayout);

        mainLayout->addWidget(new KSeparator{Qt::Horizontal, q});

        {
            ui.resultLabel = new HtmlLabel{q};
            ui.resultLabel->setFocusPolicy(Qt::ClickFocus);
            labelHelper.addLabel(ui.resultLabel);
            mainLayout->addWidget(ui.resultLabel);
        }

        mainLayout->addWidget(new KSeparator{Qt::Horizontal, q});

        mainLayout->addStretch(1);

        ui.buttonBox = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, q};

        mainLayout->addWidget(ui.buttonBox);

        connect(ui.buttonBox, &QDialogButtonBox::accepted, q, [this]() { checkAccept(); });
        connect(ui.buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);

        updateResultLabel();
    }

    QString name() const
    {
        return ui.nameInput->widget()->text().trimmed();
    }

    QString email() const
    {
        return ui.emailInput->widget()->text().trimmed();
    }

private:
    void checkAccept()
    {
        QStringList errors;
        if (ui.resultLabel->text().isEmpty()
                && !ui.nameInput->isRequired() && !ui.emailInput->isRequired()) {
            errors.push_back(i18n("Name and email address cannot both be empty."));
        }
        if (ui.nameInput->isRequired() && !ui.nameInput->hasValue()) {
            errors.push_back(i18n("A name is required."));
        } else if (!ui.nameInput->hasAcceptableInput()) {
            errors.push_back(i18n("The entered name is not valid."));
        }
        if (ui.emailInput->isRequired() && !ui.emailInput->hasValue()) {
            errors.push_back(i18n("An email address is required."));
        } else if (!ui.emailInput->hasAcceptableInput()) {
            errors.push_back(i18n("The entered email address is not valid."));
        }
        if (errors.size() > 1) {
            KMessageBox::errorList(q, i18n("Sorry, the entered data is not acceptable."), errors);
        } else if (!errors.empty()) {
            KMessageBox::sorry(q, errors.first());
        } else {
            q->accept();
        }
    }

    void updateResultLabel()
    {
        ui.resultLabel->setHtml(i18nc("@info",
            "<div>This is how the new user ID will be stored in the certificate:</div>"
            "<center><strong>%1</strong></center>",
            buildUserId(name(), email()).toHtmlEscaped()));
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
    d->ui.nameInput->widget()->setText(name);
}

QString AddUserIDDialog::name() const
{
    return d->name();
}

void AddUserIDDialog::setEmail(const QString &email)
{
    d->ui.emailInput->widget()->setText(email);
}

QString AddUserIDDialog::email() const
{
    return d->email();
}

QString AddUserIDDialog::userID() const
{
    return d->ui.resultLabel->text();
}
