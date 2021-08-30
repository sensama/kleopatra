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

using namespace Kleo;

UrlLabel::UrlLabel(QWidget *parent)
    : QLabel{parent}
{
    setTextFormat(Qt::RichText);
    setTextInteractionFlags(Qt::TextBrowserInteraction);
}

void UrlLabel::setUrl(const QUrl &url, const QString &text)
{
    // we prepend a zero-width-space character to work around a bug in QLabel::focusNextPrevChild(false)
    // which makes it impossible to leave the label with Shift+Tab if the text starts with a link
    static const QString templateString{QLatin1String{"&#8203;<a href=\"%1\">%2</a>"}};

    if (url.isEmpty()) {
        clear();
        return;
    }

    setText(templateString.arg(url.url(QUrl::FullyEncoded), text.isEmpty() ? url.toDisplayString().toHtmlEscaped() : text.toHtmlEscaped()));
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
