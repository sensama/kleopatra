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

class ScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    explicit ScrollArea(QWidget *widget = nullptr);
    ~ScrollArea() override;

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
};

}


