/*  view/formtextinput.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "formtextinput.h"

#include "errorlabel.h"
#include "utils/accessibility.h"

#include <KLocalizedString>

#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QValidator>

#include "kleopatra_debug.h"

namespace Kleo::_detail
{

class FormTextInputBase::Private
{
    FormTextInputBase *q;
public:
    Private(FormTextInputBase *q)
        : q{q}
        , mErrorMessage{i18n("Error: The entered text is not valid.")}
    {}

    QString errorMessage() const;
    void updateError();
    void updateAccessibleNameAndDescription();

    QPointer<QLabel> mLabel;
    QPointer<QWidget> mWidget;
    QPointer<ErrorLabel> mErrorLabel;
    QPointer<const QValidator> mValidator;
    QString mAccessibleName;
    QString mAccessibleDescription;
    QString mErrorMessage;
    bool mEditingInProgress = false;
};

QString FormTextInputBase::Private::errorMessage() const
{
    return q->hasAcceptableInput() ? QString{} : mErrorMessage;
}

void FormTextInputBase::Private::updateError()
{
    if (!mErrorLabel) {
        return;
    }
    const auto currentErrorMessage = mErrorLabel->text();
    const auto newErrorMessage = errorMessage();
    if (newErrorMessage == currentErrorMessage) {
        return;
    }
    if (currentErrorMessage.isEmpty() && mEditingInProgress) {
        // delay showing the error message until editing is finished, so that we
        // do not annoy the user with an error message while they are still
        // entering the recipient;
        // on the other hand, we clear the error message immediately if it does
        // not apply anymore and we update the error message immediately if it
        // changed
        return;
    }
    mErrorLabel->setVisible(!newErrorMessage.isEmpty());
    mErrorLabel->setText(newErrorMessage);
    updateAccessibleNameAndDescription();
}

void FormTextInputBase::Private::updateAccessibleNameAndDescription()
{
    // fall back to default accessible name/description if accessible name/description wasn't set explicitly
    if (mAccessibleName.isEmpty()) {
        mAccessibleName = getAccessibleName(mWidget);
    }
    if (mAccessibleDescription.isEmpty()) {
        mAccessibleDescription = getAccessibleDescription(mWidget);
    }
    const bool errorShown = mErrorLabel && mErrorLabel->isVisible();

    // Qt does not support "described-by" relations (like WCAG's "aria-describedby" relationship attribute);
    // emulate this by adding the error message to the accessible description of the input field
    const auto description = errorShown ? mAccessibleDescription + QLatin1String{" "} + mErrorLabel->text()
                                        : mAccessibleDescription;
    if (mWidget && mWidget->accessibleDescription() != description) {
        mWidget->setAccessibleDescription(description);
    }

    // Qt does not support IA2's "invalid entry" state (like WCAG's "aria-invalid" state attribute);
    // screen readers say something like "invalid entry" if this state is set;
    // emulate this by adding "invalid entry" to the accessible name of the input field
    // and its label
    const auto name = errorShown ? mAccessibleName + QLatin1String{", "} + invalidEntryText()
                                 : mAccessibleName;
    if (mLabel && mLabel->accessibleName() != name) {
        mLabel->setAccessibleName(name);
    }
    if (mWidget && mWidget->accessibleName() != name) {
        mWidget->setAccessibleName(name);
    }
}

FormTextInputBase::FormTextInputBase()
    : d{new Private{this}}
{
}

FormTextInputBase::~FormTextInputBase() = default;

QWidget *FormTextInputBase::widget() const
{
    return d->mWidget;
}

QLabel *FormTextInputBase::label() const
{
    return d->mLabel;
}

ErrorLabel *FormTextInputBase::errorLabel() const
{
    return d->mErrorLabel;
}

void FormTextInputBase::setValidator(const QValidator *validator)
{
    d->mValidator = validator;
}

void FormTextInputBase::setErrorMessage(const QString &text)
{
    if (text.isEmpty()) {
        d->mErrorMessage = i18n("Error: The entered text is not valid.");
    } else {
        d->mErrorMessage = text;
    }
}

void FormTextInputBase::setToolTip(const QString &toolTip)
{
    if (d->mLabel) {
        d->mLabel->setToolTip(toolTip);
    }
    if (d->mWidget) {
        d->mWidget->setToolTip(toolTip);
    }
}

void FormTextInputBase::setAccessibleName(const QString &name)
{
    d->mAccessibleName = name;
    d->updateAccessibleNameAndDescription();
}

void FormTextInputBase::setAccessibleDescription(const QString &description)
{
    d->mAccessibleDescription = description;
    d->updateAccessibleNameAndDescription();
}

void FormTextInputBase::setWidget(QWidget *widget)
{
    auto parent = widget ? widget->parentWidget() : nullptr;
    d->mWidget = widget;
    d->mLabel = new QLabel{parent};
    d->mErrorLabel = new ErrorLabel{parent};
    if (d->mLabel) {
        d->mLabel->setBuddy(d->mWidget);
    }
    if (d->mErrorLabel) {
        d->mErrorLabel->setVisible(false);
    }
    connectWidget();
}

void FormTextInputBase::setEnabled(bool enabled)
{
    if (d->mLabel) {
        d->mLabel->setEnabled(enabled);
    }
    if (d->mWidget) {
        d->mWidget->setEnabled(enabled);
    }
    if (d->mErrorLabel) {
        d->mErrorLabel->setVisible(enabled && !d->mErrorLabel->text().isEmpty());
    }
}

bool FormTextInputBase::validate(const QString &text, int pos) const
{
    QString textCopy = text;
    if (d->mValidator && d->mValidator->validate(textCopy, pos) != QValidator::Acceptable)
    {
        return false;
    }
    return true;
}

void FormTextInputBase::onTextChanged()
{
    d->mEditingInProgress = true;
    d->updateError();
}

void FormTextInputBase::onEditingFinished()
{
    d->mEditingInProgress = false;
    d->updateError();
}

}

template<>
bool Kleo::FormTextInput<QLineEdit>::hasAcceptableInput() const
{
    const auto w = widget();
    return w && validate(w->text(), w->cursorPosition());
}

template<>
void Kleo::FormTextInput<QLineEdit>::connectWidget()
{
    const auto w = widget();
    QObject::connect(w, &QLineEdit::editingFinished,
                     w, [this]() { onEditingFinished(); });
    QObject::connect(w, &QLineEdit::textChanged,
                     w, [this]() { onTextChanged(); });
}
