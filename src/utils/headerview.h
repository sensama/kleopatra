/* -*- mode: c++; c-basic-offset:4 -*-
    utils/headerview.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHeaderView>

#include <utils/pimpl_ptr.h>

#include <vector>

namespace Kleo
{

class HeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit HeaderView(Qt::Orientation o, QWidget *parent = nullptr);
    ~HeaderView();

    void setSectionSizes(const std::vector<int> &sizes);
    std::vector<int> sectionSizes() const;

private:
    //@{
    /*! Defined, but not implemented, to catch at least some usage errors */
    void setResizeMode(int, ResizeMode);
    //@}
private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void _klhv_slotSectionCountChanged(int, int))
    Q_PRIVATE_SLOT(d, void _klhv_slotSectionResized(int, int, int))
};

}

