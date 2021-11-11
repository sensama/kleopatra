/*
    conf/groupsconfigwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "groupsconfigwidget.h"

#include "commands/exportgroupscommand.h"
#include "dialogs/editgroupdialog.h"

#include <Libkleo/KeyCache>
#include <Libkleo/KeyGroup>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeyListSortFilterProxyModel>

#include <KLocalizedString>
#include <KRandom>

#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Dialogs;

Q_DECLARE_METATYPE(KeyGroup)

namespace
{

class ProxyModel : public AbstractKeyListSortFilterProxyModel
{
    Q_OBJECT
public:
    ProxyModel(QObject *parent = nullptr)
        : AbstractKeyListSortFilterProxyModel(parent)
    {
    }

    ~ProxyModel() override = default;

    ProxyModel *clone() const override
    {
        // compiler-generated copy ctor is fine!
        return new ProxyModel(*this);
    }
};

}

class GroupsConfigWidget::Private
{
    friend class ::Kleo::GroupsConfigWidget;
    GroupsConfigWidget *const q;

    struct {
        QLineEdit *groupsFilter = nullptr;
        QListView *groupsList = nullptr;
        QPushButton *newButton = nullptr;
        QPushButton *editButton = nullptr;
        QPushButton *deleteButton = nullptr;
        QPushButton *exportButton = nullptr;
    } ui;
    AbstractKeyListModel *groupsModel = nullptr;
    ProxyModel *groupsFilterModel = nullptr;

public:
    Private(GroupsConfigWidget *qq)
        : q(qq)
    {
        auto mainLayout = new QVBoxLayout(q);

        auto groupsLayout = new QGridLayout();
        groupsLayout->setColumnStretch(0, 1);
        groupsLayout->setRowStretch(1, 1);

        ui.groupsFilter = new QLineEdit();
        ui.groupsFilter->setClearButtonEnabled(true);
        ui.groupsFilter->setPlaceholderText(i18nc("Placeholder text", "Search..."));
        groupsLayout->addWidget(ui.groupsFilter, 0, 0);

        groupsModel = AbstractKeyListModel::createFlatKeyListModel(q);
        groupsFilterModel = new ProxyModel(q);
        groupsFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        groupsFilterModel->setFilterKeyColumn(KeyList::Summary);
        groupsFilterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        groupsFilterModel->setSourceModel(groupsModel);
        groupsFilterModel->sort(KeyList::Summary, Qt::AscendingOrder);
        ui.groupsList = new QListView();
        ui.groupsList->setModel(groupsFilterModel);
        ui.groupsList->setModelColumn(KeyList::Summary);
        ui.groupsList->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui.groupsList->setSelectionMode(QAbstractItemView::SingleSelection);

        groupsLayout->addWidget(ui.groupsList, 1, 0);

        auto groupsButtonLayout = new QVBoxLayout();

        ui.newButton = new QPushButton(i18n("New"));
        groupsButtonLayout->addWidget(ui.newButton);

        ui.editButton = new QPushButton(i18n("Edit"));
        ui.editButton->setEnabled(false);
        groupsButtonLayout->addWidget(ui.editButton);

        ui.deleteButton = new QPushButton(i18n("Delete"));
        ui.deleteButton->setEnabled(false);
        groupsButtonLayout->addWidget(ui.deleteButton);

        ui.exportButton = new QPushButton{i18nc("@action::button", "Export")};
        ui.exportButton->setEnabled(false);
        groupsButtonLayout->addWidget(ui.exportButton);

        groupsButtonLayout->addStretch(1);

        groupsLayout->addLayout(groupsButtonLayout, 1, 1);

        mainLayout->addLayout(groupsLayout, /*stretch=*/ 1);

        connect(ui.groupsFilter, &QLineEdit::textChanged, groupsFilterModel, &QSortFilterProxyModel::setFilterFixedString);
        connect(ui.groupsList->selectionModel(), &QItemSelectionModel::selectionChanged,
                q, [this] () { selectionChanged(); });
        connect(ui.groupsList, &QListView::doubleClicked,
                q, [this] (const QModelIndex &index) { editGroup(index); });
        connect(ui.newButton, &QPushButton::clicked, q, [this] () { addGroup(); });
        connect(ui.editButton, &QPushButton::clicked, q, [this] () { editGroup(); });
        connect(ui.deleteButton, &QPushButton::clicked, q, [this] () { deleteGroup(); });
        connect(ui.exportButton, &QPushButton::clicked, q, [this] () { exportGroup(); });
    }

    ~Private()
    {
    }

private:
    QModelIndex selectedIndex()
    {
        const QModelIndexList selected = ui.groupsList->selectionModel()->selectedRows();
        return selected.empty() ? QModelIndex() : selected[0];
    }

    KeyGroup getGroup(const QModelIndex &index)
    {
        return index.isValid() ? ui.groupsList->model()->data(index, KeyList::GroupRole).value<KeyGroup>() : KeyGroup();
    }

    void selectionChanged()
    {
        const KeyGroup selectedGroup = getGroup(selectedIndex());
        const bool selectedGroupIsEditable = !selectedGroup.isNull() && !selectedGroup.isImmutable();
        ui.editButton->setEnabled(selectedGroupIsEditable);
        ui.deleteButton->setEnabled(selectedGroupIsEditable);
        ui.exportButton->setEnabled(!selectedGroup.isNull());
    }

    KeyGroup showEditGroupDialog(KeyGroup group, const QString &windowTitle, EditGroupDialog::FocusWidget focusWidget)
    {
        auto dialog = std::make_unique<EditGroupDialog>(q);
        dialog->setWindowTitle(windowTitle);
        dialog->setGroupName(group.name());
        const KeyGroup::Keys &keys = group.keys();
        dialog->setGroupKeys(std::vector<GpgME::Key>(keys.cbegin(), keys.cend()));
        dialog->setInitialFocus(focusWidget);

        const int result = dialog->exec();
        if (result == QDialog::Rejected) {
            return KeyGroup();
        }

        group.setName(dialog->groupName());
        group.setKeys(dialog->groupKeys());

        return group;
    }

    void addGroup()
    {
        const KeyGroup::Id newId = KRandom::randomString(8);
        KeyGroup group = KeyGroup(newId, i18nc("default name for new group of keys", "New Group"), {}, KeyGroup::ApplicationConfig);
        group.setIsImmutable(false);

        const KeyGroup newGroup = showEditGroupDialog(
            group, i18nc("@title:window a group of keys", "New Group"), EditGroupDialog::GroupName);
        if (newGroup.isNull()) {
            return;
        }

        const QModelIndex newIndex = groupsModel->addGroup(newGroup);
        if (!newIndex.isValid()) {
            qCDebug(KLEOPATRA_LOG) << "Adding group to model failed";
            return;
        }

        Q_EMIT q->changed();
    }

    void editGroup(const QModelIndex &index = QModelIndex())
    {
        const QModelIndex groupIndex = index.isValid() ? index : selectedIndex();
        if (!groupIndex.isValid()) {
            qCDebug(KLEOPATRA_LOG) << "selection is empty";
            return;
        }
        const KeyGroup group = getGroup(groupIndex);
        if (group.isNull()) {
            qCDebug(KLEOPATRA_LOG) << "selected group is null";
            return;
        }
        if (group.isImmutable()) {
            qCDebug(KLEOPATRA_LOG) << "selected group is immutable";
            return;
        }

        const KeyGroup updatedGroup = showEditGroupDialog(
            group, i18nc("@title:window a group of keys", "Edit Group"), EditGroupDialog::KeysFilter);
        if (updatedGroup.isNull()) {
            return;
        }

        const bool success = ui.groupsList->model()->setData(groupIndex, QVariant::fromValue(updatedGroup));
        if (!success) {
            qCDebug(KLEOPATRA_LOG) << "Updating group in model failed";
            return;
        }

        Q_EMIT q->changed();
    }

    void deleteGroup()
    {
        const QModelIndex groupIndex = selectedIndex();
        if (!groupIndex.isValid()) {
            qCDebug(KLEOPATRA_LOG) << "selection is empty";
            return;
        }
        const KeyGroup group = getGroup(groupIndex);
        if (group.isNull()) {
            qCDebug(KLEOPATRA_LOG) << "selected group is null";
            return;
        }

        const bool success = groupsModel->removeGroup(group);
        if (!success) {
            qCDebug(KLEOPATRA_LOG) << "Removing group from model failed";
            return;
        }

        Q_EMIT q->changed();
    }

    void exportGroup()
    {
        const QModelIndex groupIndex = selectedIndex();
        if (!groupIndex.isValid()) {
            qCDebug(KLEOPATRA_LOG) << "selection is empty";
            return;
        }
        const KeyGroup group = getGroup(groupIndex);
        if (group.isNull()) {
            qCDebug(KLEOPATRA_LOG) << "selected group is null";
            return;
        }

        // execute export group command
        auto cmd = new ExportGroupsCommand({group});
        cmd->start();
    }
};

GroupsConfigWidget::GroupsConfigWidget(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{
}

GroupsConfigWidget::~GroupsConfigWidget() = default;

void GroupsConfigWidget::setGroups(const std::vector<KeyGroup> &groups)
{
    d->groupsModel->setGroups(groups);
}

std::vector<KeyGroup> GroupsConfigWidget::groups() const
{
    std::vector<KeyGroup> result;
    result.reserve(d->groupsModel->rowCount());
    for (int row = 0; row < d->groupsModel->rowCount(); ++row) {
        const QModelIndex index = d->groupsModel->index(row, 0);
        result.push_back(d->groupsModel->group(index));
    }
    return result;
}

#include "groupsconfigwidget.moc"
