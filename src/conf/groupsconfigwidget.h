/*
    conf/groupsconfigwidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_CONF_GROUPSCONFIGWIDGET_H__
#define __KLEOPATRA_CONF_GROUPSCONFIGWIDGET_H__

#include <QWidget>

#include <vector>

namespace Kleo
{
class KeyGroup;

class GroupsConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GroupsConfigWidget(QWidget *parent = nullptr);
    ~GroupsConfigWidget() override;

    void setGroups(const std::vector<KeyGroup> &groups);
    std::vector<KeyGroup> groups() const;

Q_SIGNALS:
    void changed();

private:
    class Private;
    const std::unique_ptr<Private> d;
};

}

#endif // __KLEOPATRA_CONF_GROUPSCONFIGWIDGET_H__
