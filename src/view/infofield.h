/*  view/infofield.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>

class QAction;
class QHBoxLayout;
class QIcon;
class QLabel;
class QLayout;
class QPushButton;
class QWidget;

namespace Kleo
{

class InfoField
{
public:
    InfoField(const QString &label, QWidget *parent);

    QLabel *label() const;
    QLayout *layout() const;

    void setValue(const QString &value, const QString &accessibleValue = {});
    QString value() const;

    void setIcon(const QIcon &icon);
    void setAction(const QAction *action);
    void setToolTip(const QString &toolTip);
    void setVisible(bool visible);

private:
    void onActionChanged();

    QLabel *mLabel = nullptr;
    QHBoxLayout *mLayout = nullptr;
    QLabel *mIcon = nullptr;
    QLabel *mValue = nullptr;
    QPushButton *mButton = nullptr;
    const QAction *mAction = nullptr;
};

}
