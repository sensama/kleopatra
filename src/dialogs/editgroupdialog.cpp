/*
    dialogs/editgroupdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "editgroupdialog.h"

#include "commands/detailscommand.h"
#include "view/keytreeview.h"

#include <Libkleo/KeyListModel>

#include <KConfigGroup>
#include <KGuiItem>
#include <KLocalizedString>
#include <KSeparator>
#include <KSharedConfig>
#include <KStandardGuiItem>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;
using namespace GpgME;

Q_DECLARE_METATYPE(GpgME::Key)

class EditGroupDialog::Private
{
    friend class ::Kleo::Dialogs::EditGroupDialog;
    EditGroupDialog *const q;

    struct {
        QLineEdit *groupNameEdit = nullptr;
        QLineEdit *availableKeysFilter = nullptr;
        KeyTreeView *availableKeysList = nullptr;
        QLineEdit *groupKeysFilter = nullptr;
        KeyTreeView *groupKeysList = nullptr;
        QDialogButtonBox *buttonBox = nullptr;
    } ui;
    AbstractKeyListModel *availableKeysModel = nullptr;
    AbstractKeyListModel *groupKeysModel = nullptr;

public:
    Private(EditGroupDialog *qq)
        : q(qq)
    {
        auto mainLayout = new QVBoxLayout(q);

        auto groupNameLayout = new QHBoxLayout();
        groupNameLayout->addWidget(new QLabel(i18nc("Name of a group of keys", "Name:")));
        ui.groupNameEdit = new QLineEdit();
        groupNameLayout->addWidget(ui.groupNameEdit);
        mainLayout->addLayout(groupNameLayout);

        mainLayout->addWidget(new KSeparator(Qt::Horizontal));

        auto centerLayout = new QVBoxLayout();

        auto availableKeysLayout = new QVBoxLayout();
        availableKeysLayout->addWidget(new QLabel(i18n("Available keys:")));

        ui.availableKeysFilter = new QLineEdit();
        ui.availableKeysFilter->setClearButtonEnabled(true);
        ui.availableKeysFilter->setPlaceholderText(i18nc("Placeholder text", "Search..."));
        availableKeysLayout->addWidget(ui.availableKeysFilter);

        availableKeysModel = AbstractKeyListModel::createFlatKeyListModel(q);
        availableKeysModel->useKeyCache(true, KeyList::AllKeys);
        ui.availableKeysList = new KeyTreeView(q);
        ui.availableKeysList->view()->setRootIsDecorated(false);
        ui.availableKeysList->setFlatModel(availableKeysModel);
        ui.availableKeysList->setHierarchicalView(false);
        availableKeysLayout->addWidget(ui.availableKeysList, /*stretch=*/ 1);

        centerLayout->addLayout(availableKeysLayout, /*stretch=*/ 1);

        auto buttonsLayout = new QHBoxLayout();
        buttonsLayout->addStretch(1);

        auto addButton = new QPushButton();
        addButton->setIcon(QIcon::fromTheme(QStringLiteral("arrow-down")));
        addButton->setToolTip(i18n("Add the selected keys to the group"));
        addButton->setEnabled(false);
        buttonsLayout->addWidget(addButton);

        auto removeButton = new QPushButton();
        removeButton->setIcon(QIcon::fromTheme(QStringLiteral("arrow-up")));
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
        ui.groupKeysList = new KeyTreeView(q);
        ui.groupKeysList->view()->setRootIsDecorated(false);
        ui.groupKeysList->setFlatModel(groupKeysModel);
        ui.groupKeysList->setHierarchicalView(false);
        groupKeysLayout->addWidget(ui.groupKeysList, /*stretch=*/ 1);

        centerLayout->addLayout(groupKeysLayout, /*stretch=*/ 1);

        mainLayout->addLayout(centerLayout);

        mainLayout->addWidget(new KSeparator(Qt::Horizontal));

        ui.buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QPushButton *okButton = ui.buttonBox->button(QDialogButtonBox::Ok);
        KGuiItem::assign(okButton, KStandardGuiItem::ok());
        KGuiItem::assign(ui.buttonBox->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());
        okButton->setEnabled(false);
        mainLayout->addWidget(ui.buttonBox);

        connect(ui.groupNameEdit, &QLineEdit::textChanged,
                q, [okButton] (const QString &text) {
                    okButton->setEnabled(!text.trimmed().isEmpty());
                });
        connect(ui.availableKeysFilter, &QLineEdit::textChanged,
                ui.availableKeysList, &KeyTreeView::setStringFilter);
        connect(ui.availableKeysList->view()->selectionModel(), &QItemSelectionModel::selectionChanged,
                q, [addButton] (const QItemSelection &selected, const QItemSelection &) {
                    addButton->setEnabled(!selected.isEmpty());
                });
        connect(ui.availableKeysList->view(), &QAbstractItemView::doubleClicked,
                q, [this] (const QModelIndex &index) { showKeyDetails(index); });
        connect(ui.groupKeysFilter, &QLineEdit::textChanged,
                ui.groupKeysList, &KeyTreeView::setStringFilter);
        connect(ui.groupKeysList->view()->selectionModel(), &QItemSelectionModel::selectionChanged,
                q, [removeButton] (const QItemSelection &selected, const QItemSelection &) {
                    removeButton->setEnabled(!selected.isEmpty());
                });
        connect(ui.groupKeysList->view(), &QAbstractItemView::doubleClicked,
                q, [this] (const QModelIndex &index) { showKeyDetails(index); });
        connect(addButton, &QPushButton::clicked, q, [this] () { addKeysToGroup(); });
        connect(removeButton, &QPushButton::clicked, q, [this] () { removeKeysFromGroup(); });
        connect(ui.buttonBox, &QDialogButtonBox::accepted, q, &EditGroupDialog::accept);
        connect(ui.buttonBox, &QDialogButtonBox::rejected, q, &EditGroupDialog::reject);

        // calculate default size with enough space for the key list
        const auto fm = q->fontMetrics();
        const QSize sizeHint = q->sizeHint();
        const QSize defaultSize = QSize(qMax(sizeHint.width(), 150 * fm.horizontalAdvance(QLatin1Char('x'))),
                                        sizeHint.height());
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

        KConfigGroup availableKeysConfig = configGroup.group("AvailableKeysView");
        ui.availableKeysList->saveLayout(availableKeysConfig);

        KConfigGroup groupKeysConfig = configGroup.group("GroupKeysView");
        ui.groupKeysList->saveLayout(groupKeysConfig);

        configGroup.sync();
    }

    void restoreLayout(const QSize &defaultSize)
    {
        const KConfigGroup configGroup(KSharedConfig::openConfig(), "EditGroupDialog");

        const KConfigGroup availableKeysConfig = configGroup.group("AvailableKeysView");
        ui.availableKeysList->restoreLayout(availableKeysConfig);

        const KConfigGroup groupKeysConfig = configGroup.group("GroupKeysView");
        ui.groupKeysList->restoreLayout(groupKeysConfig);

        const QSize size = configGroup.readEntry("Size", defaultSize);
        if (size.isValid()) {
            q->resize(size);
        }
    }

    void showKeyDetails(const QModelIndex &index)
    {
        if (!index.isValid()) {
            return;
        }
        const GpgME::Key key = index.model()->data(index, KeyList::KeyRole).value<GpgME::Key>();
        if (!key.isNull()) {
            auto cmd = new DetailsCommand(key, nullptr);
            cmd->setParentWidget(q);
            cmd->start();
        }
    }

    void addKeysToGroup();
    void removeKeysFromGroup();
};

void EditGroupDialog::Private::addKeysToGroup()
{
    const std::vector<Key> selectedGroupKeys = ui.groupKeysList->selectedKeys();

    const std::vector<Key> selectedKeys = ui.availableKeysList->selectedKeys();
    groupKeysModel->addKeys(selectedKeys);

    ui.groupKeysList->selectKeys(selectedGroupKeys);
}

void EditGroupDialog::Private::removeKeysFromGroup()
{
    const std::vector<Key> selectedKeys = ui.groupKeysList->selectedKeys();
    for (const Key &key : selectedKeys) {
        groupKeysModel->removeKey(key);
    }
}

EditGroupDialog::EditGroupDialog(QWidget *parent)
    : QDialog(parent)
    , d(new Private(this))
{
    setWindowTitle(i18nc("@title:window", "Edit Group"));
}

EditGroupDialog::~EditGroupDialog() = default;

void EditGroupDialog::setInitialFocus(FocusWidget widget)
{
    switch (widget) {
    case GroupName:
        d->ui.groupNameEdit->setFocus();
        break;
    case KeysFilter:
        d->ui.availableKeysFilter->setFocus();
        break;
    default:
        qCDebug(KLEOPATRA_LOG) << "EditGroupDialog::setInitialFocus - invalid focus widget:" << widget;
    }
}

void EditGroupDialog::setGroupName(const QString &name)
{
    d->ui.groupNameEdit->setText(name);
}

QString EditGroupDialog::groupName() const
{
    return d->ui.groupNameEdit->text().trimmed();
}

void EditGroupDialog::setGroupKeys(const std::vector<Key> &keys)
{
    d->groupKeysModel->setKeys(keys);
}

std::vector<Key> EditGroupDialog::groupKeys() const
{
    std::vector<Key> keys;
    keys.reserve(d->groupKeysModel->rowCount());
    for (int row = 0; row < d->groupKeysModel->rowCount(); ++row) {
        const QModelIndex index = d->groupKeysModel->index(row, 0);
        keys.push_back(d->groupKeysModel->key(index));
    }
    return keys;
}
