/*
    dialogs/editgroupdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_DIALOGS_EDITGROUPDIALOG_H__
#define __KLEOPATRA_DIALOGS_EDITGROUPDIALOG_H__

#include <QDialog>

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

private:
    class Private;
    const std::unique_ptr<Private> d;
};

}
}

#endif // __KLEOPATRA_DIALOGS_EDITGROUPDIALOG_H__
