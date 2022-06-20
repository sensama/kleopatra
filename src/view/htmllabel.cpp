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
#include <QDesktopServices>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

using namespace Kleo;

namespace
{
struct AnchorData {
    int start;
    int end;
    QString text;
    QString href;
};
}

class HtmlLabel::Private
{
    HtmlLabel *q;
public:
    Private(HtmlLabel *q) : q{q} {}

    void updateText(const QString &newText = {});

    std::vector<AnchorData> &anchors();
    int anchorIndex(int start);
    void invalidateAnchorCache();

    bool mAnchorsValid = false;
    std::vector<AnchorData> mAnchors;
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
    invalidateAnchorCache();
}

std::vector<AnchorData> &HtmlLabel::Private::anchors()
{
    if (mAnchorsValid) {
        return mAnchors;
    }

    mAnchors.clear();

    QTextDocument doc;
    doc.setHtml(q->text());

    // taken from QWidgetTextControl::setFocusToNextOrPreviousAnchor and QWidgetTextControl::findNextPrevAnchor
    for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
        QTextBlock::Iterator it = block.begin();

        while (!it.atEnd()) {
            const QTextFragment fragment = it.fragment();
            const QTextCharFormat fmt = fragment.charFormat();

            if (fmt.isAnchor() && fmt.hasProperty(QTextFormat::AnchorHref)) {
                const int anchorStart = fragment.position();
                const QString anchorHref = fmt.anchorHref();
                int anchorEnd = -1;

                // find next non-anchor fragment
                for (; !it.atEnd(); ++it) {
                    const QTextFragment fragment = it.fragment();
                    const QTextCharFormat fmt = fragment.charFormat();

                    if (!fmt.isAnchor() || fmt.anchorHref() != anchorHref) {
                        anchorEnd = fragment.position();
                        break;
                    }
                }

                if (anchorEnd == -1) {
                    anchorEnd = block.position() + block.length() - 1;
                }

                QTextCursor cursor{&doc};
                cursor.setPosition(anchorStart);
                cursor.setPosition(anchorEnd, QTextCursor::KeepAnchor);
                QString anchorText = cursor.selectedText();
                mAnchors.push_back({anchorStart, anchorEnd, anchorText, anchorHref});
            } else {
                ++it;
            }
        }
    }

    mAnchorsValid = true;
    return mAnchors;
}

int HtmlLabel::Private::anchorIndex(int start)
{
    anchors(); // ensure that the anchor cache is valid
    auto it = std::find_if(std::cbegin(mAnchors), std::cend(mAnchors), [start](const auto &anchor) {
        return anchor.start == start;
    });
    if (it != std::cend(mAnchors)) {
        return std::distance(std::cbegin(mAnchors), it);
    }
    return -1;
}

void HtmlLabel::Private::invalidateAnchorCache()
{
    mAnchorsValid = false;
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
        d->invalidateAnchorCache();
        return;
    }
    d->updateText(html);
}

void HtmlLabel::setLinkColor(const QColor &color)
{
    d->linkColor = color;
    d->updateText();
}

int HtmlLabel::numberOfAnchors() const
{
    return d->anchors().size();
}

QString HtmlLabel::anchorText(int index) const
{
    if (index >= 0 && index < numberOfAnchors()) {
        return d->anchors()[index].text;
    }
    return {};
}

QString HtmlLabel::anchorHref(int index) const
{
    if (index >= 0 && index < numberOfAnchors()) {
        return d->anchors()[index].href;
    }
    return {};
}

void HtmlLabel::activateAnchor(int index)
{
    // based on QWidgetTextControlPrivate::activateLinkUnderCursor
    if (index < 0 || index >= numberOfAnchors()) {
        return;
    }
    const auto &anchor = d->anchors()[index];
    if (anchor.href.isEmpty()) {
        return;
    }
    if (hasFocus()) {
        // move cursor just before the anchor and clear the selection
        setSelection(anchor.start, 0);
        // focus the anchor
        focusNextPrevChild(true);
    } else {
        // clear the selection moving the cursor just after the anchor
        setSelection(anchor.end, 0);
    }
    if (openExternalLinks()) {
        QDesktopServices::openUrl(QUrl{anchor.href});
    } else {
        Q_EMIT linkActivated(anchor.href);
    }
}

int HtmlLabel::selectedAnchor() const
{
    return d->anchorIndex(selectionStart());
}

bool HtmlLabel::focusNextPrevChild(bool next)
{
    const bool result = QLabel::focusNextPrevChild(next);
    if (hasFocus() && QAccessible::isActive()) {
        const int anchorIndex = selectedAnchor();
        if (anchorIndex >= 0) {
            QAccessibleEvent focusEvent(this, QAccessible::Focus);
            focusEvent.setChild(anchorIndex);
            QAccessible::updateAccessibility(&focusEvent);
        }
    }
    return result;
}
