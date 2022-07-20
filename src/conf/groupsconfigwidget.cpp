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

#include <Libkleo/Debug>
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

struct Selection
{
    KeyGroup current;
    std::vector<KeyGroup> selected;
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

        auto groupsLayout = new QGridLayout;
        groupsLayout->setColumnStretch(0, 1);
        groupsLayout->setRowStretch(1, 1);

        ui.groupsFilter = new QLineEdit(q);
        ui.groupsFilter->setClearButtonEnabled(true);
        ui.groupsFilter->setPlaceholderText(i18nc("@info::placeholder", "Search..."));
        groupsLayout->addWidget(ui.groupsFilter, 0, 0);

        groupsModel = AbstractKeyListModel::createFlatKeyListModel(q);
        groupsFilterModel = new ProxyModel(q);
        groupsFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        groupsFilterModel->setFilterKeyColumn(KeyList::Summary);
        groupsFilterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        groupsFilterModel->setSourceModel(groupsModel);
        groupsFilterModel->sort(KeyList::Summary, Qt::AscendingOrder);
        ui.groupsList = new QListView(q);
        ui.groupsList->setModel(groupsFilterModel);
        ui.groupsList->setModelColumn(KeyList::Summary);
        ui.groupsList->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui.groupsList->setSelectionMode(QAbstractItemView::ExtendedSelection);

        groupsLayout->addWidget(ui.groupsList, 1, 0);

        auto groupsButtonLayout = new QVBoxLayout;

        ui.newButton = new QPushButton(i18nc("@action::button", "New"), q);
        groupsButtonLayout->addWidget(ui.newButton);

        ui.editButton = new QPushButton(i18nc("@action::button", "Edit"), q);
        ui.editButton->setEnabled(false);
        groupsButtonLayout->addWidget(ui.editButton);

        ui.deleteButton = new QPushButton(i18nc("@action::button", "Delete"), q);
        ui.deleteButton->setEnabled(false);
        groupsButtonLayout->addWidget(ui.deleteButton);

        ui.exportButton = new QPushButton{i18nc("@action::button", "Export"), q};
        ui.exportButton->setEnabled(false);
        groupsButtonLayout->addWidget(ui.exportButton);

        groupsButtonLayout->addStretch(1);

        groupsLayout->addLayout(groupsButtonLayout, 1, 1);

        mainLayout->addLayout(groupsLayout, /*stretch=*/ 1);

        connect(ui.groupsFilter, &QLineEdit::textChanged, q, [this](const auto &s) {
            groupsFilterModel->setFilterRegularExpression(QRegularExpression::escape(s));
        });
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
    auto getGroupIndex(const KeyGroup &group)
    {
        QModelIndex index;
        if (const KeyListModelInterface *const klmi = dynamic_cast<KeyListModelInterface *>(ui.groupsList->model())) {
            index = klmi->index(group);
        }
        return index;
    }

    auto selectedRows()
    {
        return ui.groupsList->selectionModel()->selectedRows();
    }

    auto getGroup(const QModelIndex &index)
    {
        return index.isValid() ? ui.groupsList->model()->data(index, KeyList::GroupRole).value<KeyGroup>() : KeyGroup{};
    }

    auto getGroups(const QModelIndexList &indexes)
    {
        std::vector<KeyGroup> groups;
        std::transform(std::begin(indexes), std::end(indexes),
                       std::back_inserter(groups),
                       [this](const auto &index) { return getGroup(index); });
        return groups;
    }

    Selection saveSelection()
    {
        return {getGroup(ui.groupsList->selectionModel()->currentIndex()), getGroups(selectedRows())};
    }

    void restoreSelection(const Selection &selection)
    {
        auto selectionModel = ui.groupsList->selectionModel();
        selectionModel->clearSelection();
        for (const auto &group : selection.selected) {
            selectionModel->select(getGroupIndex(group), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        auto currentIndex = getGroupIndex(selection.current);
        if (currentIndex.isValid()) {
            // keep current item if old current group is gone
            selectionModel->setCurrentIndex(currentIndex, QItemSelectionModel::NoUpdate);
        }
    }

    void selectionChanged()
    {
        const auto selectedGroups = getGroups(selectedRows());
        const bool allSelectedGroupsAreEditable =
            std::all_of(std::begin(selectedGroups), std::end(selectedGroups),
                        [](const auto &g) { return !g.isNull() && !g.isImmutable(); });
        ui.editButton->setEnabled(selectedGroups.size() == 1 && allSelectedGroupsAreEditable);
        ui.deleteButton->setEnabled(!selectedGroups.empty() && allSelectedGroupsAreEditable);
        ui.exportButton->setEnabled(selectedGroups.size() == 1);
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

    void editGroup(const QModelIndex &index = {})
    {
        QModelIndex groupIndex;
        if (index.isValid()) {
            groupIndex = index;
        } else {
            const auto selection = selectedRows();
            if (selection.size() != 1) {
                qCDebug(KLEOPATRA_LOG) << (selection.empty() ? "selection is empty" : "more than one group is selected");
                return;
            }
            groupIndex = selection.front();
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

        // look up index of updated group; the groupIndex used above may have become invalid
        const auto updatedGroupIndex = getGroupIndex(updatedGroup);
        if (!updatedGroupIndex.isValid()) {
            qCDebug(KLEOPATRA_LOG) << __func__ << "Failed to find index of group" << updatedGroup;
            return;
        }
        const bool success = ui.groupsList->model()->setData(updatedGroupIndex, QVariant::fromValue(updatedGroup));
        if (!success) {
            qCDebug(KLEOPATRA_LOG) << "Updating group in model failed";
            return;
        }

        Q_EMIT q->changed();
    }

    void deleteGroup()
    {
        const auto selectedGroups = getGroups(selectedRows());
        if (selectedGroups.empty()) {
            qCDebug(KLEOPATRA_LOG) << "selection is empty";
            return;
        }

        for (const auto &group : selectedGroups) {
            const bool success = groupsModel->removeGroup(group);
            if (!success) {
                qCDebug(KLEOPATRA_LOG) << "Removing group from model failed:" << group;
            }
        }

        Q_EMIT q->changed();
    }

    void exportGroup()
    {
        const auto selectedGroups = getGroups(selectedRows());
        if (selectedGroups.empty()) {
            qCDebug(KLEOPATRA_LOG) << "selection is empty";
            return;
        }

        // execute export group command
        auto cmd = new ExportGroupsCommand(selectedGroups);
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
    const auto selection = d->saveSelection();
    d->groupsModel->setGroups(groups);
    d->restoreSelection(selection);
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
