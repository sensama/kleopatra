/* -*- mode: c++; c-basic-offset:4 -*-
    utils/scrollarea.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QScrollArea>

namespace Kleo
{

/**
 * This class improves a few aspects of QScrollArea for usage by us, in
 * particular, for vertically scrollable widgets.
 */
class ScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    /**
     * Creates a scroll area with a QWidget with QVBoxLayout that is flagged
     * as resizable.
     */
    explicit ScrollArea(QWidget *parent = nullptr);
    ~ScrollArea() override;

    /**
     * Reimplemented to add the minimum size hint of the widget.
     */
    QSize minimumSizeHint() const override;

    /**
     * Reimplemented to remove the caching of the size/size hint of the
     * widget and to add the horizontal size hint of the vertical scroll bar
     * unless it is explicitly turned off.
     */
    QSize sizeHint() const override;
};

}


