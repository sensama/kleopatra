/*
    conf/groupsconfigdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_CONF_GROUPSCONFIGDIALOG_H__
#define __KLEOPATRA_CONF_GROUPSCONFIGDIALOG_H__

#include <KConfigDialog>

class GroupsConfigDialog : public KConfigDialog
{
    Q_OBJECT
public:
    explicit GroupsConfigDialog(QWidget *parent = nullptr);
    ~GroupsConfigDialog() override;

    static QString dialogName();

private Q_SLOTS:
    void updateSettings() override;
    void updateWidgets() override;

private:
    bool hasChanged() override;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

#endif /* __KLEOPATRA_CONF_GROUPSCONFIGDIALOG_H__ */
