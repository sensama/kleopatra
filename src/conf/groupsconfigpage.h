/*
    conf/groupsconfigpage.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_CONF_GROUPSCONFIGPAGE_H__
#define __KLEOPATRA_CONF_GROUPSCONFIGPAGE_H__

#include <KCModule>

class GroupsConfigPage : public KCModule
{
    Q_OBJECT
public:
    explicit GroupsConfigPage(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    ~GroupsConfigPage() override;

    void load() override;
    void save() override;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

#endif // __KLEOPATRA_CONF_GROUPSCONFIGPAGE_H__
