/*
    accessibility/accessiblerichtextlabel.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "accessiblerichtextlabel_p.h"

#include <QLabel>
#include <QTextDocument>

using namespace Kleo;

AccessibleRichTextLabel::AccessibleRichTextLabel(QWidget *w)
    : QAccessibleWidget{w, QAccessible::StaticText}
{
    Q_ASSERT(qobject_cast<QLabel *>(w));
}

void *AccessibleRichTextLabel::interface_cast(QAccessible::InterfaceType t)
{
    if (t == QAccessible::TextInterface)
        return static_cast<QAccessibleTextInterface *>(this);
    return QAccessibleWidget::interface_cast(t);
}

QAccessible::State AccessibleRichTextLabel::state() const
{
    QAccessible::State state = QAccessibleWidget::state();
    state.readOnly = true;
    state.selectableText = true;
    return state;
}

QString AccessibleRichTextLabel::text(QAccessible::Text t) const
{
    QString str;
    switch (t) {
    case QAccessible::Name:
        str = widget()->accessibleName();
        if (str.isEmpty()) {
            str = displayText();
        }
        break;
    default:
        break;
    }
    if (str.isEmpty())
        str = QAccessibleWidget::text(t);
    return str;
}

void AccessibleRichTextLabel::selection(int selectionIndex, int *startOffset, int *endOffset) const
{
    *startOffset = *endOffset = 0;
    if (selectionIndex != 0)
        return;

    *startOffset = label()->selectionStart();
    *endOffset = *startOffset + label()->selectedText().size();
}

int AccessibleRichTextLabel::selectionCount() const
{
    return label()->hasSelectedText() ? 1 : 0;
}

void AccessibleRichTextLabel::addSelection(int startOffset, int endOffset)
{
    setSelection(0, startOffset, endOffset);
}

void AccessibleRichTextLabel::removeSelection(int selectionIndex)
{
    if (selectionIndex != 0)
        return;

    label()->setSelection(-1, -1);
}

void AccessibleRichTextLabel::setSelection(int selectionIndex, int startOffset, int endOffset)
{
    if (selectionIndex != 0)
        return;

    label()->setSelection(startOffset, endOffset - startOffset);
}

int AccessibleRichTextLabel::cursorPosition() const
{
    return label()->hasSelectedText() ? label()->selectionStart() + label()->selectedText().size() : 0;
}

void AccessibleRichTextLabel::setCursorPosition(int position)
{
    Q_UNUSED(position)
}

QString AccessibleRichTextLabel::text(int startOffset, int endOffset) const
{
    if (startOffset > endOffset)
        return {};

    // most likely the client is asking for the selected text, so return it
    // instead of a slice of displayText() if the offsets match the selection
    if (startOffset == label()->selectionStart()
            && endOffset == startOffset + label()->selectedText().size()) {
        return label()->selectedText();
    }
    return displayText().mid(startOffset, endOffset - startOffset);
}

int AccessibleRichTextLabel::characterCount() const
{
    return displayText().size();
}

QRect AccessibleRichTextLabel::characterRect(int offset) const
{
    Q_UNUSED(offset)
    return {};
}

int AccessibleRichTextLabel::offsetAtPoint(const QPoint &point) const
{
    Q_UNUSED(point)
    return -1;
}

QString AccessibleRichTextLabel::attributes(int offset, int *startOffset, int *endOffset) const
{
    *startOffset = *endOffset = offset;
    return {};
}

void AccessibleRichTextLabel::scrollToSubstring(int startIndex, int endIndex)
{
    Q_UNUSED(startIndex)
    Q_UNUSED(endIndex)
}

QLabel *AccessibleRichTextLabel::label() const
{
    return qobject_cast<QLabel*>(object());
}

QString AccessibleRichTextLabel::displayText() const
{
    // calculate an approximation of the displayed text without using private
    // information of QLabel
    QString str = label()->text();
    if (label()->textFormat() == Qt::RichText
            || (label()->textFormat() == Qt::AutoText && Qt::mightBeRichText(str))) {
        QTextDocument doc;
        doc.setHtml(str);
        str = doc.toPlainText();
    }
    return str;
}
