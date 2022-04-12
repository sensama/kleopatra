/*
    view/htmllabel.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "htmllabel.h"
#include "utils/accessibility.h"

#include <QAccessible>

using namespace Kleo;

class HtmlLabel::Private
{
    HtmlLabel *q;
public:
    Private(HtmlLabel *q) : q{q} {}

    void updateText(const QString &newText = {});

    QColor linkColor;
};

void HtmlLabel::Private::updateText(const QString &newText)
{
    static const QString styleTemplate{QLatin1String{"<style type=\"text/css\">a {color: %1;}</style>"}};

    if (newText.isEmpty() && q->text().isEmpty()) {
        return;
    }

    const auto styleTag = styleTemplate.arg(linkColor.isValid() ? linkColor.name() : q->palette().link().color().name());
    if (newText.isEmpty()) {
        q->setText(styleTag + q->text().mid(styleTag.size()));
    } else {
        q->setText(styleTag + newText);
    }
}

HtmlLabel::HtmlLabel(QWidget *parent)
    : HtmlLabel{{}, parent}
{
}

HtmlLabel::HtmlLabel(const QString &html, QWidget *parent)
    : QLabel{parent}
    , d{new Private{this}}
{
    setTextFormat(Qt::RichText);
    setTextInteractionFlags(Qt::TextBrowserInteraction);
    setHtml(html);
}

HtmlLabel::~HtmlLabel() = default;

void HtmlLabel::setHtml(const QString &html)
{
    if (html.isEmpty()) {
        clear();
        return;
    }
    d->updateText(html);
}

void HtmlLabel::setLinkColor(const QColor &color)
{
    d->linkColor = color;
    d->updateText();
}

void HtmlLabel::focusInEvent(QFocusEvent *ev)
{
    QLabel::focusInEvent(ev);

    // if the text label gets focus, then select its text; this is a workaround
    // for missing focus indicators for labels in many Qt styles
    const Qt::FocusReason reason = ev->reason();
    const auto isKeyboardFocusEvent = reason == Qt::TabFocusReason
                                   || reason == Qt::BacktabFocusReason
                                   || reason == Qt::ShortcutFocusReason;
    if (!text().isEmpty() && isKeyboardFocusEvent) {
        Kleo::selectLabelText(this);
    }
}

bool HtmlLabel::focusNextPrevChild(bool next)
{
    const bool result = QLabel::focusNextPrevChild(next);
    if (hasFocus() && hasSelectedText()) {
        QAccessibleTextSelectionEvent ev(this, selectionStart(), selectionStart() + selectedText().size());
        QAccessible::updateAccessibility(&ev);
    }
    return result;
}
