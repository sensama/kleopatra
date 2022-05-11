/* -*- mode: c++; c-basic-offset:4 -*-
    utils/scrollarea.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "scrollarea.h"

#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QVBoxLayout>

using namespace Kleo;

ScrollArea::ScrollArea(QWidget *parent)
    : QScrollArea{parent}
{
    auto w = new QWidget;
    w->setObjectName(QLatin1String("scrollarea_widget"));
    new QVBoxLayout{w};
    setWidget(w);
    setWidgetResizable(true);
    w->installEventFilter(this);
}

ScrollArea::~ScrollArea()
{
    widget()->removeEventFilter(this);
}

void ScrollArea::setMaximumAutoAdjustHeight(int maxHeight)
{
    mMaximumAutoAdjustHeight = maxHeight;
}

int ScrollArea::maximumAutoAdjustHeight() const
{
    if (mMaximumAutoAdjustHeight < 0) {
        // if no height is set then use 2/3 of the desktop's height, i.e.
        // the same as Qt uses for top-level widgets
        return screen()->availableGeometry().height() * 2 / 3;
    }
    return mMaximumAutoAdjustHeight;
}

QSize ScrollArea::minimumSizeHint() const
{
    const int fw = frameWidth();
    QSize sz{2 * fw, 2 * fw};
    sz += widget()->minimumSizeHint();
    if (verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff) {
        sz.setWidth(sz.width() + verticalScrollBar()->sizeHint().width());
    }
    if (horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff) {
        sz.setHeight(sz.height() + horizontalScrollBar()->sizeHint().height());
    }
    return QScrollArea::minimumSizeHint().expandedTo(sz);
}

QSize ScrollArea::sizeHint() const
{
    const int fw = frameWidth();
    QSize sz{2 * fw, 2 * fw};
    sz += viewportSizeHint();
    if (verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff) {
        sz.setWidth(sz.width() + verticalScrollBar()->sizeHint().width());
    }
    if (horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff) {
        sz.setHeight(sz.height() + horizontalScrollBar()->sizeHint().height());
    }
    sz = QScrollArea::sizeHint().expandedTo(sz);
    return sz;
}

bool ScrollArea::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() == QEvent::Resize && obj == widget() && sizeAdjustPolicy() == AdjustToContents) {
        const auto *const event = static_cast<QResizeEvent*>(ev);
        if (event->size().height() > event->oldSize().height()) {
            const auto currentViewportHeight = viewport()->height();
            const auto wantedViewportHeight = std::min(event->size().height(), maximumAutoAdjustHeight());
            if (currentViewportHeight < wantedViewportHeight) {
                setMinimumHeight(height() - currentViewportHeight + wantedViewportHeight);
            }
        }
    }
    return QScrollArea::eventFilter(obj, ev);
}
