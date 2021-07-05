/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/certificateselectiondialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "certificateselectiondialog.h"

#include "settings.h"

#include "conf/groupsconfigdialog.h"

#include <view/keytreeview.h>
#include <view/searchbar.h>
#include <view/tabwidget.h>

#include "utils/tags.h"

#include <Libkleo/KeyGroup>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeyCache>

#include <commands/reloadkeyscommand.h>
#include <commands/lookupcertificatescommand.h>
#include <commands/newcertificatecommand.h>
#include <commands/importcertificatefromfilecommand.h>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include <KConfigDialog>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QItemSelectionModel>
#include <QAbstractItemView>
#include <QPointer>
#include <QVBoxLayout>

#include <algorithm>

#include <gpgme++/gpgmepp_version.h>
#if GPGMEPP_VERSION >= 0x10E00 // 1.14.0
# define GPGME_HAS_REMARKS
#endif

using namespace Kleo;
using namespace Kleo::Dialogs;
using namespace Kleo::Commands;
using namespace GpgME;


CertificateSelectionDialog::Option CertificateSelectionDialog::optionsFromProtocol(Protocol proto)
{
    switch (proto) {
    case OpenPGP: return CertificateSelectionDialog::OpenPGPFormat;
    case CMS:     return CertificateSelectionDialog::CMSFormat;
    default:      return CertificateSelectionDialog::AnyFormat;
    }
}

class CertificateSelectionDialog::Private
{
    friend class ::Kleo::Dialogs::CertificateSelectionDialog;
    CertificateSelectionDialog *const q;
public:
    explicit Private(CertificateSelectionDialog *qq);

private:
    void reload()
    {
        Command *const cmd = new ReloadKeysCommand(nullptr);
        cmd->setParentWidget(q);
        cmd->start();
    }
    void create()
    {
        auto cmd = new NewCertificateCommand(nullptr);
        cmd->setParentWidget(q);
        if ((options & AnyFormat) != AnyFormat) {
            cmd->setProtocol((options & OpenPGPFormat) ? OpenPGP : CMS);
        }
        cmd->start();
    }
    void lookup()
    {
        Command *const cmd = new LookupCertificatesCommand(nullptr);
        cmd->setParentWidget(q);
        cmd->start();
    }
    void manageGroups()
    {
        KConfigDialog *dialog = KConfigDialog::exists(GroupsConfigDialog::dialogName());
        if (dialog) {
            // reparent the dialog to ensure it's shown on top of the modal CertificateSelectionDialog
            dialog->setParent(q, Qt::Dialog);
        } else {
            dialog = new GroupsConfigDialog(q);
        }
        dialog->show();
    }
    void slotKeysMayHaveChanged();
    void slotCurrentViewChanged(QAbstractItemView *newView);
    void slotSelectionChanged();
    void slotDoubleClicked(const QModelIndex &idx);

private:
    bool acceptable(const std::vector<Key> &keys, const std::vector<KeyGroup> &groups)
    {
        return !keys.empty() || !groups.empty();
    }
    void updateLabelText()
    {
        ui.label.setText(!customLabelText.isEmpty() ? customLabelText :
                         (options & MultiSelection)
                         ? i18n("Please select one or more of the following certificates:")
                         : i18n("Please select one of the following certificates:"));
    }

private:
    QSet<QAbstractItemView *> connectedViews;
    QString customLabelText;
    Options options = AnyCertificate | AnyFormat;

    struct UI {
        QLabel label;
        SearchBar searchBar;
        TabWidget tabWidget;
        QDialogButtonBox buttonBox;
    } ui;

    void setUpUI(CertificateSelectionDialog *q)
    {
        KDAB_SET_OBJECT_NAME(ui.label);
        KDAB_SET_OBJECT_NAME(ui.searchBar);
        KDAB_SET_OBJECT_NAME(ui.tabWidget);
        KDAB_SET_OBJECT_NAME(ui.buttonBox);

        auto vlay = new QVBoxLayout(q);
        vlay->addWidget(&ui.label);
        vlay->addWidget(&ui.searchBar);
        vlay->addWidget(&ui.tabWidget, 1);
        vlay->addWidget(&ui.buttonBox);

        QPushButton *const okButton = ui.buttonBox.addButton(QDialogButtonBox::Ok);
        okButton->setEnabled(false);
        ui.buttonBox.addButton(QDialogButtonBox::Close);
        QPushButton *const reloadButton = ui.buttonBox.addButton(i18n("Reload"), QDialogButtonBox::ActionRole);
        QPushButton *const importButton = ui.buttonBox.addButton(i18n("Import..."), QDialogButtonBox::ActionRole);
        QPushButton *const lookupButton = ui.buttonBox.addButton(i18n("Lookup..."), QDialogButtonBox::ActionRole);
        QPushButton *const createButton = ui.buttonBox.addButton(i18n("New..."), QDialogButtonBox::ActionRole);
        QPushButton *const groupsButton = ui.buttonBox.addButton(i18n("Groups..."), QDialogButtonBox::ActionRole);
        groupsButton->setVisible(Settings().groupsEnabled());

        importButton->setToolTip(i18nc("@info:tooltip", "Import certificate from file"));
        lookupButton->setToolTip(i18nc("@info:tooltip", "Lookup certificates on server"));
        reloadButton->setToolTip(i18nc("@info:tooltip", "Refresh certificate list"));
        createButton->setToolTip(i18nc("@info:tooltip", "Create a new certificate"));
        groupsButton->setToolTip(i18nc("@info:tooltip", "Manage certificate groups"));

        connect(&ui.buttonBox, &QDialogButtonBox::accepted, q, &CertificateSelectionDialog::accept);
        connect(&ui.buttonBox, &QDialogButtonBox::rejected, q, &CertificateSelectionDialog::reject);
        connect(reloadButton, &QPushButton::clicked, q, [this] () { reload(); });
        connect(lookupButton, &QPushButton::clicked, q, [this] () { lookup(); });
        connect(createButton, &QPushButton::clicked, q, [this] () { create(); });
        connect(groupsButton, &QPushButton::clicked, q, [this] () { manageGroups(); });
        connect(KeyCache::instance().get(), &KeyCache::keysMayHaveChanged,
                q, [this] () { slotKeysMayHaveChanged(); });

        connect(importButton, &QPushButton::clicked, q, [importButton, q] () {
            importButton->setEnabled(false);
            auto cmd = new Kleo::ImportCertificateFromFileCommand();
            connect(cmd, &Kleo::ImportCertificateFromFileCommand::finished,
                    q, [importButton]() {
                importButton->setEnabled(true);
            });
            cmd->setParentWidget(q);
            cmd->start();
        });
    }
};

CertificateSelectionDialog::Private::Private(CertificateSelectionDialog *qq)
    : q(qq)
{
    setUpUI(q);
    ui.tabWidget.setFlatModel(AbstractKeyListModel::createFlatKeyListModel(q));
    ui.tabWidget.setHierarchicalModel(AbstractKeyListModel::createHierarchicalKeyListModel(q));
#ifdef GPGME_HAS_REMARKS
    const auto tagKeys = Tags::tagKeys();
    ui.tabWidget.flatModel()->setRemarkKeys(tagKeys);
    ui.tabWidget.hierarchicalModel()->setRemarkKeys(tagKeys);
#endif
    ui.tabWidget.connectSearchBar(&ui.searchBar);

    connect(&ui.tabWidget, &TabWidget::currentViewChanged,
            q, [this] (QAbstractItemView *view) { slotCurrentViewChanged(view); });

    updateLabelText();
    q->setWindowTitle(i18nc("@title:window", "Certificate Selection"));
}

CertificateSelectionDialog::CertificateSelectionDialog(QWidget *parent)
    : QDialog(parent), d(new Private(this))
{
    const KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("kleopatracertificateselectiondialogrc"));
    d->ui.tabWidget.loadViews(config.data());
    const KConfigGroup geometry(config, "Geometry");
    resize(geometry.readEntry("size", size()));
    d->slotKeysMayHaveChanged();
}

CertificateSelectionDialog::~CertificateSelectionDialog() {}

void CertificateSelectionDialog::setCustomLabelText(const QString &txt)
{
    if (txt == d->customLabelText) {
        return;
    }
    d->customLabelText = txt;
    d->updateLabelText();
}

QString CertificateSelectionDialog::customLabelText() const
{
    return d->customLabelText;
}

void CertificateSelectionDialog::setOptions(Options options)
{
    Q_ASSERT((options & CertificateSelectionDialog::AnyCertificate) != 0);
    Q_ASSERT((options & CertificateSelectionDialog::AnyFormat) != 0);
    if (d->options == options) {
        return;
    }
    d->options = options;

    d->ui.tabWidget.setMultiSelection(options & MultiSelection);

    d->slotKeysMayHaveChanged();
}

CertificateSelectionDialog::Options CertificateSelectionDialog::options() const
{
    return d->options;
}

void CertificateSelectionDialog::setStringFilter(const QString &filter)
{
    d->ui.tabWidget.setStringFilter(filter);
}

void CertificateSelectionDialog::setKeyFilter(const std::shared_ptr<KeyFilter> &filter)
{
    d->ui.tabWidget.setKeyFilter(filter);
}

namespace
{

void selectRows(const QAbstractItemView *view, const QModelIndexList &indexes)
{
    if (!view) {
        return;
    }
    QItemSelectionModel *const sm = view->selectionModel();
    Q_ASSERT(sm);

    for (const QModelIndex &idx : std::as_const(indexes)) {
        if (idx.isValid()) {
            sm->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }
}

QModelIndexList getGroupIndexes(const KeyListModelInterface *model, const std::vector<KeyGroup> &groups)
{
    QModelIndexList indexes;
    indexes.reserve(groups.size());
    std::transform(groups.begin(), groups.end(),
                   std::back_inserter(indexes),
                   [model] (const KeyGroup &group) {
                       return model->index(group);
                   });
    indexes.erase(std::remove_if(indexes.begin(), indexes.end(),
                                 [] (const QModelIndex &index) { return !index.isValid(); }),
                  indexes.end());
    return indexes;
}

}

void CertificateSelectionDialog::selectCertificates(const std::vector<Key> &keys)
{
    const auto *const model = d->ui.tabWidget.currentModel();
    Q_ASSERT(model);
    selectRows(d->ui.tabWidget.currentView(), model->indexes(keys));
}

void CertificateSelectionDialog::selectCertificate(const Key &key)
{
    selectCertificates(std::vector<Key>(1, key));
}

void CertificateSelectionDialog::selectGroups(const std::vector<KeyGroup> &groups)
{
    const auto *const model = d->ui.tabWidget.currentModel();
    Q_ASSERT(model);
    selectRows(d->ui.tabWidget.currentView(), getGroupIndexes(model, groups));
}

namespace
{

QModelIndexList getSelectedRows(const QAbstractItemView *view)
{
    if (!view) {
        return {};
    }
    const QItemSelectionModel *const sm = view->selectionModel();
    Q_ASSERT(sm);
    return sm->selectedRows();
}

std::vector<KeyGroup> getGroups(const KeyListModelInterface *model, const QModelIndexList &indexes)
{
    std::vector<KeyGroup> groups;
    groups.reserve(indexes.size());
    std::transform(indexes.begin(), indexes.end(),
                   std::back_inserter(groups),
                   [model](const QModelIndex &idx) {
                       return model->group(idx);
                   });
    groups.erase(std::remove_if(groups.begin(), groups.end(), std::mem_fn(&Kleo::KeyGroup::isNull)), groups.end());
    return groups;
}

}

std::vector<Key> CertificateSelectionDialog::selectedCertificates() const
{
    const KeyListModelInterface *const model = d->ui.tabWidget.currentModel();
    Q_ASSERT(model);
    return model->keys(getSelectedRows(d->ui.tabWidget.currentView()));
}

Key CertificateSelectionDialog::selectedCertificate() const
{
    const std::vector<Key> keys = selectedCertificates();
    return keys.empty() ? Key() : keys.front();
}

std::vector<KeyGroup> CertificateSelectionDialog::selectedGroups() const
{
    const KeyListModelInterface *const model = d->ui.tabWidget.currentModel();
    Q_ASSERT(model);
    return getGroups(model, getSelectedRows(d->ui.tabWidget.currentView()));
}

void CertificateSelectionDialog::hideEvent(QHideEvent *e)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("kleopatracertificateselectiondialogrc"));
    d->ui.tabWidget.saveViews(config.data());
    KConfigGroup geometry(config, "Geometry");
    geometry.writeEntry("size", size());
    QDialog::hideEvent(e);
}

void CertificateSelectionDialog::Private::slotKeysMayHaveChanged()
{
    q->setEnabled(true);
    std::vector<Key> keys = (options & SecretKeys) ? KeyCache::instance()->secretKeys() : KeyCache::instance()->keys();
    q->filterAllowedKeys(keys, options);
    const std::vector<KeyGroup> groups = (options & IncludeGroups) ? KeyCache::instance()->groups() : std::vector<KeyGroup>();

    const std::vector<Key> selectedKeys = q->selectedCertificates();
    const std::vector<KeyGroup> selectedGroups = q->selectedGroups();
    if (AbstractKeyListModel *const model = ui.tabWidget.flatModel()) {
        model->setKeys(keys);
        model->setGroups(groups);
    }
    if (AbstractKeyListModel *const model = ui.tabWidget.hierarchicalModel()) {
        model->setKeys(keys);
        model->setGroups(groups);
    }
    q->selectCertificates(selectedKeys);
    q->selectGroups(selectedGroups);
}

void CertificateSelectionDialog::filterAllowedKeys(std::vector<Key> &keys, int options)
{
    auto end = keys.end();

    switch (options & AnyFormat) {
    case OpenPGPFormat:
        end = std::remove_if(keys.begin(), end, [](const Key &key) { return key.protocol() != OpenPGP; });
        break;
    case CMSFormat:
        end = std::remove_if(keys.begin(), end, [](const Key &key) { return key.protocol() != CMS; });
        break;
    default:
    case AnyFormat:
        ;
    }

    switch (options & AnyCertificate) {
    case SignOnly:
        end = std::remove_if(keys.begin(), end, [](const Key &key) { return !key.canReallySign(); });
        break;
    case EncryptOnly:
        end = std::remove_if(keys.begin(), end, [](const Key &key) { return !key.canEncrypt(); });
        break;
    default:
    case AnyCertificate:
        ;
    }

    if (options & SecretKeys) {
        end = std::remove_if(keys.begin(), end, [](const Key &key) { return !key.hasSecret(); });
    }

    keys.erase(end, keys.end());
}

void CertificateSelectionDialog::Private::slotCurrentViewChanged(QAbstractItemView *newView)
{
    if (!connectedViews.contains(newView)) {
        connectedViews.insert(newView);
        connect(newView, &QAbstractItemView::doubleClicked,
                q, [this] (const QModelIndex &index) { slotDoubleClicked(index); });
        Q_ASSERT(newView->selectionModel());
        connect(newView->selectionModel(), &QItemSelectionModel::selectionChanged,
                q, [this] (const QItemSelection &, const QItemSelection &) { slotSelectionChanged(); });
    }
    slotSelectionChanged();
}

void CertificateSelectionDialog::Private::slotSelectionChanged()
{
    if (QPushButton *const pb = ui.buttonBox.button(QDialogButtonBox::Ok)) {
        pb->setEnabled(acceptable(q->selectedCertificates(), q->selectedGroups()));
    }
}

void CertificateSelectionDialog::Private::slotDoubleClicked(const QModelIndex &idx)
{
    QAbstractItemView *const view = ui.tabWidget.currentView();
    Q_ASSERT(view);
    const auto *const model = ui.tabWidget.currentModel();
    Q_ASSERT(model);
    Q_UNUSED(model)
    QItemSelectionModel *const sm = view->selectionModel();
    Q_ASSERT(sm);
    sm->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QMetaObject::invokeMethod(q, [this]() {q->accept();}, Qt::QueuedConnection);
}

void CertificateSelectionDialog::accept()
{
    if (d->acceptable(selectedCertificates(), selectedGroups())) {
        QDialog::accept();
    }
}

#include "moc_certificateselectiondialog.cpp"
