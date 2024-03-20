// SPDX-FileCopyrightText: 2024 g10 Code GmbH
// SPDX-FileContributor: Tobias Fella <tobias.fella@gnupg.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

namespace Kleo
{
namespace Config
{

class KleoConfigModule : public QWidget
{
    Q_OBJECT

public:
    using QWidget::QWidget;
    virtual void save() = 0;
    virtual void load() = 0;
    virtual void defaults() = 0;

Q_SIGNALS:
    void changed();
};

}
}
