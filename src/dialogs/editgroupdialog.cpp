/*
    dialogs/editgroupdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "editgroupdialog.h"

#include <Libkleo/KeyCache>
#include <Libkleo/KeyGroup>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeyListSortFilterProxyModel>

#include <KConfigGroup>
#include <KGuiItem>
#include <KLocalizedString>
#include <KSeparator>
#include <KSharedConfig>
#include <KStandardGuiItem>

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QVBoxLayout>

#include <Libkleo/Formatting>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Dialogs;
using namespace GpgME;

Q_DECLARE_METATYPE(GpgME::Key)

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

class EditGroupDialog::Private
{
    friend class ::Kleo::Dialogs::EditGroupDialog;
    EditGroupDialog *const q;

    struct {
        QLabel *groupNameLabel = nullptr;
        QLineEdit *availableKeysFilter = nullptr;
        QListView *availableKeysList = nullptr;
        QLineEdit *groupKeysFilter = nullptr;
        QListView *groupKeysList = nullptr;
        QDialogButtonBox *buttonBox = nullptr;
    } ui;
    AbstractKeyListModel *availableKeysModel = nullptr;
    ProxyModel *availableKeysFilterModel = nullptr;
    AbstractKeyListModel *groupKeysModel = nullptr;
    ProxyModel *groupKeysFilterModel = nullptr;
    KeyGroup group;

public:
    Private(EditGroupDialog *qq)
        : q(qq)
    {
        auto mainLayout = new QVBoxLayout(q);

        ui.groupNameLabel = new QLabel();
        ui.groupNameLabel->setWordWrap(true);
        mainLayout->addWidget(ui.groupNameLabel);

        mainLayout->addWidget(new KSeparator(Qt::Horizontal));

        auto centerLayout = new QHBoxLayout();

        auto availableKeysLayout = new QVBoxLayout();
        availableKeysLayout->addWidget(new QLabel(i18n("Available keys:")));

        ui.availableKeysFilter = new QLineEdit();
        ui.availableKeysFilter->setClearButtonEnabled(true);
        ui.availableKeysFilter->setPlaceholderText(i18nc("Placeholder text", "Search..."));
        availableKeysLayout->addWidget(ui.availableKeysFilter);

        availableKeysModel = AbstractKeyListModel::createFlatKeyListModel(q);
        availableKeysModel->useKeyCache(true, KeyList::AllKeys);
        availableKeysFilterModel = new ProxyModel(q);
        availableKeysFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        availableKeysFilterModel->setFilterKeyColumn(KeyList::Summary);
        availableKeysFilterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        availableKeysFilterModel->setSourceModel(availableKeysModel);
        availableKeysFilterModel->sort(KeyList::Summary, Qt::AscendingOrder);
        ui.availableKeysList = new QListView();
        ui.availableKeysList->setModel(availableKeysFilterModel);
        ui.availableKeysList->setModelColumn(KeyList::Summary);
        ui.availableKeysList->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui.availableKeysList->setSelectionMode(QAbstractItemView::ExtendedSelection);
        availableKeysLayout->addWidget(ui.availableKeysList, /*stretch=*/ 1);

        centerLayout->addLayout(availableKeysLayout, /*stretch=*/ 1);

        auto buttonsLayout = new QVBoxLayout();
        buttonsLayout->addStretch(1);

        auto addButton = new QPushButton();
        addButton->setIcon(QIcon::fromTheme(QStringLiteral("arrow-right")));
        addButton->setToolTip(i18n("Add the selected keys to the group"));
        addButton->setEnabled(false);
        buttonsLayout->addWidget(addButton);

        auto removeButton = new QPushButton();
        removeButton->setIcon(QIcon::fromTheme(QStringLiteral("arrow-left")));
        removeButton->setToolTip(i18n("Remove the selected keys from the group"));
        removeButton->setEnabled(false);
        buttonsLayout->addWidget(removeButton);

        buttonsLayout->addStretch(1);

        centerLayout->addLayout(buttonsLayout);

        auto groupKeysLayout = new QVBoxLayout();
        groupKeysLayout->addWidget(new QLabel(i18n("Group keys:")));

        ui.groupKeysFilter = new QLineEdit();
        ui.groupKeysFilter->setClearButtonEnabled(true);
        ui.groupKeysFilter->setPlaceholderText(i18nc("Placeholder text", "Search..."));
        groupKeysLayout->addWidget(ui.groupKeysFilter);

        groupKeysModel = AbstractKeyListModel::createFlatKeyListModel(q);
        groupKeysFilterModel = new ProxyModel(q);
        groupKeysFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        groupKeysFilterModel->setFilterKeyColumn(KeyList::Summary);
        groupKeysFilterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        groupKeysFilterModel->setSourceModel(groupKeysModel);
        groupKeysFilterModel->sort(KeyList::Summary, Qt::AscendingOrder);
        ui.groupKeysList = new QListView();
        ui.groupKeysList->setModel(groupKeysFilterModel);
        ui.groupKeysList->setModelColumn(KeyList::Summary);
        ui.groupKeysList->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui.groupKeysList->setSelectionMode(QAbstractItemView::ExtendedSelection);
        groupKeysLayout->addWidget(ui.groupKeysList, /*stretch=*/ 1);

        centerLayout->addLayout(groupKeysLayout, /*stretch=*/ 1);

        mainLayout->addLayout(centerLayout);

        mainLayout->addWidget(new KSeparator(Qt::Horizontal));

        ui.buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        KGuiItem::assign(ui.buttonBox->button(QDialogButtonBox::Ok), KStandardGuiItem::ok());
        KGuiItem::assign(ui.buttonBox->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());
        mainLayout->addWidget(ui.buttonBox);

        connect(ui.availableKeysFilter, &QLineEdit::textChanged, availableKeysFilterModel, &QSortFilterProxyModel::setFilterFixedString);
        connect(ui.availableKeysList->selectionModel(), &QItemSelectionModel::selectionChanged,
                q, [addButton] (const QItemSelection &selected, const QItemSelection &) {
                    addButton->setEnabled(!selected.isEmpty());
                });
        connect(ui.groupKeysFilter, &QLineEdit::textChanged, groupKeysFilterModel, &QSortFilterProxyModel::setFilterFixedString);
        connect(ui.groupKeysList->selectionModel(), &QItemSelectionModel::selectionChanged,
                q, [removeButton] (const QItemSelection &selected, const QItemSelection &) {
                    removeButton->setEnabled(!selected.isEmpty());
                });
        connect(addButton, &QPushButton::clicked, q, [this] () { addKeysToGroup(); });
        connect(removeButton, &QPushButton::clicked, q, [this] () { removeKeysFromGroup(); });
        connect(ui.buttonBox, &QDialogButtonBox::accepted, q, &EditGroupDialog::accept);
        connect(ui.buttonBox, &QDialogButtonBox::rejected, q, &EditGroupDialog::reject);

        // calculate default size with enough space for the key list
        const auto fm = q->fontMetrics();
        const QSize sizeHint = q->sizeHint();
        const QSize defaultSize = QSize(qMax(sizeHint.width(), 150 * fm.horizontalAdvance(QLatin1Char('x'))),
                                        sizeHint.height() + 12 * fm.lineSpacing());
        restoreLayout(defaultSize);
    }

    ~Private()
    {
        saveLayout();
    }

private:
    void saveLayout()
    {
        KConfigGroup configGroup(KSharedConfig::openConfig(), "EditGroupDialog");
        configGroup.writeEntry("Size", q->size());
        configGroup.sync();
    }

    void restoreLayout(const QSize &defaultSize)
    {
        const KConfigGroup configGroup(KSharedConfig::openConfig(), "EditGroupDialog");
        const QSize size = configGroup.readEntry("Size", defaultSize);
        if (size.isValid()) {
            q->resize(size);
        }
    }

    void addKeysToGroup();
    void removeKeysFromGroup();

    void updateGroupKeysKeepingSelection();
};

namespace {
std::vector<Key> getSelectedKeys(const QListView *view)
{
    const QModelIndexList selectedRows = view->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        return std::vector<Key>();
    }
    const KeyListModelInterface *const keyListModel = dynamic_cast<KeyListModelInterface *>(view->model());
    Q_ASSERT(keyListModel);
    return keyListModel->keys(selectedRows);
}

void setSelectedKeys(const QListView *view, const std::vector<Key> &keys)
{
    const KeyListModelInterface *const keyListModel = dynamic_cast<KeyListModelInterface *>(view->model());
    Q_ASSERT(keyListModel);
    const QModelIndexList indexes = keyListModel->indexes(keys);
    for (const QModelIndex &idx : indexes) {
        if (idx.isValid()) {
            view->selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }
}
}

void EditGroupDialog::Private::addKeysToGroup()
{
    const std::vector<Key> selectedKeys = getSelectedKeys(ui.availableKeysList);
    qCDebug(KLEOPATRA_LOG) << "Number of selected keys:" << selectedKeys.size();
    const auto numKeysAdded = std::count_if(selectedKeys.cbegin(), selectedKeys.cend(),
                                            [this] (const Key &key) { return group.insert(key); });
    qCDebug(KLEOPATRA_LOG) << "Number of added keys:" << numKeysAdded;
    if (numKeysAdded > 0) {
        updateGroupKeysKeepingSelection();
    }
}

void EditGroupDialog::Private::removeKeysFromGroup()
{
    const std::vector<Key> selectedKeys = getSelectedKeys(ui.groupKeysList);
    for (const Key &key : selectedKeys) {
        group.erase(key);
    }
    const KeyGroup::Keys &keys = group.keys();
    groupKeysModel->setKeys(std::vector<GpgME::Key>(keys.cbegin(), keys.cend()));
}

void EditGroupDialog::Private::updateGroupKeysKeepingSelection()
{
    const std::vector<Key> selectedGroupKeys = getSelectedKeys(ui.groupKeysList);
    const KeyGroup::Keys &keys = group.keys();
    groupKeysModel->setKeys(std::vector<GpgME::Key>(keys.cbegin(), keys.cend()));
    setSelectedKeys(ui.groupKeysList, selectedGroupKeys);
}

EditGroupDialog::EditGroupDialog(QWidget *parent)
    : QDialog(parent)
    , d(new Private(this))
{
    setWindowTitle(i18nc("@title:window", "Edit Group"));
}

EditGroupDialog::~EditGroupDialog()
{
}

void EditGroupDialog::setGroup(const KeyGroup &group)
{
    d->group = group;
    d->ui.groupNameLabel->setText(group.name());
    const KeyGroup::Keys &keys = group.keys();
    d->groupKeysModel->setKeys(std::vector<GpgME::Key>(keys.cbegin(), keys.cend()));
}

void EditGroupDialog::accept()
{
    KeyCache::mutableInstance()->update(d->group);
    QDialog::accept();
}

#include "editgroupdialog.moc"
