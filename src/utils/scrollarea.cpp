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

namespace
{

static QSize getMinimumSizeHint(const QWidget *w)
{
    return w ? w->minimumSizeHint() : QSize(0, 0);
}

static QSize getSizeHint(const QWidget *w)
{
    return w ? w->sizeHint() : QSize(0, 0);
}
}

ScrollArea::ScrollArea(QWidget *parent) : QScrollArea(parent)
{
    setWidget(new QWidget);
    new QVBoxLayout(widget());
    setWidgetResizable(true);
}

ScrollArea::~ScrollArea()
{
}

QSize ScrollArea::minimumSizeHint() const
{
    return QSize(getMinimumSizeHint(widget()).width() + getSizeHint(verticalScrollBar()).width() + 2 * frameWidth(), 0)
           .expandedTo(QScrollArea::minimumSizeHint());
}

QSize ScrollArea::sizeHint() const
{
    const QSize widgetSizeHint = getSizeHint(widget());
    const int fw = frameWidth();
    return QScrollArea::sizeHint().expandedTo(widgetSizeHint + QSize(2 * fw, 2 * fw) + QSize(getSizeHint(verticalScrollBar()).width(), 0));
}

