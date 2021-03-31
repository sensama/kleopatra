/* -*- mode: c++; c-basic-offset:4 -*-
    conf/smimevalidationconfigurationwidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <utils/pimpl_ptr.h>

namespace Kleo
{
namespace Config
{

class SMimeValidationConfigurationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SMimeValidationConfigurationWidget(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~SMimeValidationConfigurationWidget();

public Q_SLOTS:
    void load();
    void save() const;
    void defaults();

Q_SIGNALS:
    void changed();

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void enableDisableActions())
};

}
}

