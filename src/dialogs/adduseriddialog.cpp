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
#include "utils/scrollarea.h"
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
        ScrollArea *scrollArea;
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

        {
            const auto infoText = nameIsRequired || emailIsRequired
                ? i18n("Enter a name and an email address to use for the user ID.")
                : i18n("Enter a name and/or an email address to use for the user ID.");
            auto label = new QLabel{infoText, q};
            mainLayout->addWidget(label);
        }

        mainLayout->addWidget(new KSeparator{Qt::Horizontal, q});

        ui.scrollArea = new ScrollArea{q};
        ui.scrollArea->setFocusPolicy(Qt::NoFocus);
        ui.scrollArea->setFrameStyle(QFrame::NoFrame);
        ui.scrollArea->setBackgroundRole(q->backgroundRole());
        ui.scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        ui.scrollArea->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
        auto scrollAreaLayout = qobject_cast<QBoxLayout *>(ui.scrollArea->widget()->layout());
        scrollAreaLayout->setContentsMargins(0, 0, 0, 0);

        {
            ui.nameInput = FormTextInput<QLineEdit>::create(q);
            ui.nameInput->setLabelText(i18nc("@label", "Name"));
            ui.nameInput->setIsRequired(nameIsRequired);
            ui.nameInput->setValueRequiredErrorMessage(i18n("Enter a name."));
            const auto regexp = config.readEntry("NAME_regex");
            if (regexp.isEmpty()) {
                ui.nameInput->setValidator(Validation::simpleName(Validation::Optional, q));
                ui.nameInput->setHint(i18n("Must not include <, >, and @."),
                                      i18nc("text for screen readers",
                                            "Must not include less-than sign, greater-than sign, and at sign."));
                ui.nameInput->setInvalidEntryErrorMessage(
                    i18n("The name must not include <, >, and @."),
                    i18nc("text for screen readers",
                          "The name must not include less-than sign, greater-than sign, and at sign."));
            } else {
                ui.nameInput->setValidator(Validation::simpleName(regexp, Validation::Optional, q));
                ui.nameInput->setHint(i18n("Must be in the format required by your organization and "
                                           "must not include <, >, and @."),
                                      i18nc("text for screen readers",
                                            "Must be in the format required by your organization and "
                                            "must not include less-than sign, greater-than sign, and at sign."));
                ui.nameInput->setInvalidEntryErrorMessage(
                    i18n("The name must be in the format required by your organization and "
                         "it must not include <, >, and @."),
                    i18nc("text for screen readers",
                          "The name must be in the format required by your organization and "
                          "it must not include less-than sign, greater-than sign, and at sign."));
            }

            scrollAreaLayout->addWidget(ui.nameInput->label());
            scrollAreaLayout->addWidget(ui.nameInput->hintLabel());
            scrollAreaLayout->addWidget(ui.nameInput->errorLabel());
            scrollAreaLayout->addWidget(ui.nameInput->widget());
        }
        connect(ui.nameInput->widget(), &QLineEdit::textChanged,
                q, [this]() { updateResultLabel(); });

        {
            ui.emailInput = FormTextInput<QLineEdit>::create(q);
            ui.emailInput->setLabelText(i18nc("@label", "Email address"));
            ui.emailInput->setIsRequired(emailIsRequired);
            ui.emailInput->setValueRequiredErrorMessage(i18n("Enter an email address."));
            const auto regexp = config.readEntry(QLatin1String("EMAIL_regex"));
            if (regexp.isEmpty()) {
                ui.emailInput->setValidator(Validation::email(Validation::Optional, q));
                ui.emailInput->setInvalidEntryErrorMessage(i18n(
                    "Enter an email address in the correct format, like name@example.com."));
            } else {
                ui.emailInput->setValidator(Validation::email(regexp, Validation::Optional, q));
                ui.emailInput->setHint(i18n(
                    "Must be in the format required by your organization"));
                ui.emailInput->setInvalidEntryErrorMessage(i18n(
                    "Enter an email address in the correct format required by your organization."));
            }

            scrollAreaLayout->addWidget(ui.emailInput->label());
            scrollAreaLayout->addWidget(ui.emailInput->hintLabel());
            scrollAreaLayout->addWidget(ui.emailInput->errorLabel());
            scrollAreaLayout->addWidget(ui.emailInput->widget());
        }
        connect(ui.emailInput->widget(), &QLineEdit::textChanged,
                q, [this]() { updateResultLabel(); });

        scrollAreaLayout->addWidget(new KSeparator{Qt::Horizontal, q});

        {
            ui.resultLabel = new HtmlLabel{q};
            ui.resultLabel->setFocusPolicy(Qt::ClickFocus);
            labelHelper.addLabel(ui.resultLabel);
            scrollAreaLayout->addWidget(ui.resultLabel);
        }

        scrollAreaLayout->addStretch(1);

        mainLayout->addWidget(ui.scrollArea);

        mainLayout->addWidget(new KSeparator{Qt::Horizontal, q});

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
            errors.push_back(i18n("Enter a name or an email address."));
        }
        const auto nameError = ui.nameInput->currentError();
        if (!nameError.isEmpty()) {
            errors.push_back(nameError);
        }
        const auto emailError = ui.emailInput->currentError();
        if (!emailError.isEmpty()) {
            errors.push_back(emailError);
        }
        if (errors.size() > 1) {
            KMessageBox::errorList(q, i18n("There is a problem."), errors);
        } else if (!errors.empty()) {
            KMessageBox::error(q, errors.first());
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
    return buildUserId(name(), email());
}
