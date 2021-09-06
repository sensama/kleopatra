/*
    view/urllabel.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "urllabel.h"

#include <QAccessible>
#include <QPalette>

using namespace Kleo;

class UrlLabel::Private
{
    UrlLabel *q;
public:
    Private(UrlLabel *q) : q{q} {}

    void ensureLinkColorIsValid();
    void updateLabel();

    QUrl url;
    QString text;
    QColor linkColor;
};

void UrlLabel::Private::ensureLinkColorIsValid()
{
    if (!linkColor.isValid()) {
        linkColor = q->palette().link().color();
    }
}

void UrlLabel::Private::updateLabel()
{
    // we prepend a zero-width-space character to work around a bug in QLabel::focusNextPrevChild(false)
    // which makes it impossible to leave the label with Shift+Tab if the text starts with a link
    static const QString templateString{QLatin1String{"&#8203;<a style=\"color: %1;\" href=\"%2\">%3</a>"}};

    if (url.isEmpty()) {
        q->clear();
        return;
    }

    ensureLinkColorIsValid();
    q->setText(templateString.arg(
        linkColor.name(),
        url.url(QUrl::FullyEncoded),
        text.isEmpty() ? url.toDisplayString().toHtmlEscaped() : text.toHtmlEscaped()));
}

UrlLabel::UrlLabel(QWidget *parent)
    : QLabel{parent}
    , d{new Private{this}}
{
    setTextFormat(Qt::RichText);
    setTextInteractionFlags(Qt::TextBrowserInteraction);
}

UrlLabel::~UrlLabel() = default;

void UrlLabel::setUrl(const QUrl &url, const QString &text)
{
    d->url = url;
    d->text = text;
    d->updateLabel();
}

void UrlLabel::setLinkColor(const QColor &color)
{
    d->linkColor = color;
    d->updateLabel();
}

void UrlLabel::focusInEvent(QFocusEvent *event)
{
    // immediately focus the URL when the label get focus
    QLabel::focusInEvent(event);
    if (!hasSelectedText()) {
        focusNextPrevChild(true);
    }
}

bool UrlLabel::focusNextPrevChild(bool next)
{
    const bool result = QLabel::focusNextPrevChild(next);
    if (hasFocus() && hasSelectedText()) {
        QAccessibleTextSelectionEvent ev(this, selectionStart(), selectionStart() + selectedText().size());
        QAccessible::updateAccessibility(&ev);
    }
    return result;
}
