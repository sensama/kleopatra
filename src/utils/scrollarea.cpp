/* -*- mode: c++; c-basic-offset:4 -*-
    utils/scrollarea.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "scrollarea.h"

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
}

ScrollArea::~ScrollArea() = default;

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

