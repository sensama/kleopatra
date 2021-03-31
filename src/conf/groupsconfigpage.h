/*
    conf/groupsconfigpage.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

class GroupsConfigPage : public QWidget
{
    Q_OBJECT
public:
    explicit GroupsConfigPage(QWidget *parent = nullptr);
    ~GroupsConfigPage() override;

public Q_SLOTS:
    void load();
    void save();

Q_SIGNALS:
    void changed(bool state);

private:
    class Private;
    const std::unique_ptr<Private> d;
};

