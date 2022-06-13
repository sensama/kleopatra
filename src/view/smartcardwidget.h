/*  view/smartcardwidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <interfaces/focusfirstchild.h>

#include <QWidget>

#include <memory>

namespace Kleo
{

/* SmartCardWidget a generic widget to interact with smartcards */
class SmartCardWidget: public QWidget, public FocusFirstChild
{
    Q_OBJECT
public:
    explicit SmartCardWidget(QWidget *parent = nullptr);
    ~SmartCardWidget() override;

    void focusFirstChild(Qt::FocusReason reason = Qt::OtherFocusReason) override;

public Q_SLOTS:
    void reload();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace Kleo
