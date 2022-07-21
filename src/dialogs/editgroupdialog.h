/*
    dialogs/editgroupdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

#include <memory>
#include <vector>

namespace GpgME
{
class Key;
}

namespace Kleo
{
namespace Dialogs
{

class EditGroupDialog : public QDialog
{
    Q_OBJECT
public:
    enum FocusWidget {
        GroupName,
        KeysFilter
    };

    explicit EditGroupDialog(QWidget *parent = nullptr);
    ~EditGroupDialog() override;

    void setInitialFocus(FocusWidget widget);

    void setGroupName(const QString &name);
    QString groupName() const;

    void setGroupKeys(const std::vector<GpgME::Key> &keys);
    std::vector<GpgME::Key> groupKeys() const;

protected:
    void showEvent(QShowEvent *event) override;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

}
}

