/*  SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

class QLabel;

namespace Kleo
{

class WaitWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaitWidget(QWidget *parent = nullptr);
    ~WaitWidget() override;

    void setText(const QString &text);

private:
    QLabel *mLabel;
};

} // namespace Kleo
