/* -*- mode: c++; c-basic-offset:4 -*-
    controllers/keylistcontroller.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config-kleopatra.h>

#include "keylistcontroller.h"
#include "tabwidget.h"

#include <smartcard/readerstatus.h>

#include <utils/action_data.h>

#include "tooltippreferences.h"
#include "kleopatra_debug.h"
#include "commands/exportcertificatecommand.h"
#include "commands/exportopenpgpcertstoservercommand.h"
#include "commands/exportsecretkeycommand.h"
#include "commands/importcertificatefromfilecommand.h"
#include "commands/changepassphrasecommand.h"
#include "commands/lookupcertificatescommand.h"
#include "commands/reloadkeyscommand.h"
#include "commands/refreshx509certscommand.h"
#include "commands/refreshopenpgpcertscommand.h"
#include "commands/detailscommand.h"
#include "commands/deletecertificatescommand.h"
#include "commands/decryptverifyfilescommand.h"
#include "commands/signencryptfilescommand.h"
#include "commands/signencryptfoldercommand.h"
#include "commands/clearcrlcachecommand.h"
#include "commands/dumpcrlcachecommand.h"
#include "commands/dumpcertificatecommand.h"
#include "commands/importcrlcommand.h"
#include "commands/changeexpirycommand.h"
#include "commands/changeownertrustcommand.h"
#include "commands/changeroottrustcommand.h"
#include "commands/certifycertificatecommand.h"
#include "commands/adduseridcommand.h"
#include "commands/newcertificatecommand.h"
#include "commands/checksumverifyfilescommand.h"
#include "commands/checksumcreatefilescommand.h"
#include "commands/exportpaperkeycommand.h"

#include <Libkleo/KeyCache>
#include <Libkleo/KeyListModel>
#include <Libkleo/Formatting>


#include <gpgme++/key.h>

#include <KActionCollection>
#include <KLocalizedString>

#include <QAbstractItemView>
#include <QPointer>
#include <QItemSelectionModel>
#include <QAction>

#include <algorithm>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::SmartCard;
using namespace GpgME;

class KeyListController::Private
{
    friend class ::Kleo::KeyListController;
    KeyListController *const q;
public:
    explicit Private(KeyListController *qq);
    ~Private();

    void connectView(QAbstractItemView *view);
    void connectCommand(Command *cmd);

    void connectTabWidget();
    void disconnectTabWidget();

    void addCommand(Command *cmd)
    {
        connectCommand(cmd);
        commands.insert(std::lower_bound(commands.begin(), commands.end(), cmd), cmd);
    }
    void addView(QAbstractItemView *view)
    {
        connectView(view);
        views.insert(std::lower_bound(views.begin(), views.end(), view), view);
    }
    void removeView(QAbstractItemView *view)
    {
        view->disconnect(q);
        view->selectionModel()->disconnect(q);
        views.erase(std::remove(views.begin(), views.end(), view), views.end());
    }

public:
    void slotDestroyed(QObject *o)
    {
        qCDebug(KLEOPATRA_LOG) << (void *)o;
        views.erase(std::remove(views.begin(), views.end(), o), views.end());
        commands.erase(std::remove(commands.begin(), commands.end(), o), commands.end());
    }
    void slotDoubleClicked(const QModelIndex &idx);
    void slotActivated(const QModelIndex &idx);
    void slotSelectionChanged(const QItemSelection &old, const QItemSelection &new_);
    void slotContextMenu(const QPoint &pos);
    void slotCommandFinished();
    void slotAddKey(const Key &key);
    void slotAboutToRemoveKey(const Key &key);
    void slotProgress(const QString &what, int current, int total)
    {
        Q_EMIT q->progress(current, total);
        if (!what.isEmpty()) {
            Q_EMIT q->message(what);
        }
    }
    void slotActionTriggered();
    void slotCurrentViewChanged(QAbstractItemView *view)
    {
        if (view && !std::binary_search(views.cbegin(), views.cend(), view)) {
            qCDebug(KLEOPATRA_LOG) << "you need to register view" << view << "before trying to set it as the current view!";
            addView(view);
        }
        currentView = view;
        q->enableDisableActions(view ? view->selectionModel() : nullptr);
    }

private:
    int toolTipOptions() const;

private:
    static Command::Restrictions calculateRestrictionsMask(const QItemSelectionModel *sm);

private:
    struct action_item {
        QPointer<QAction> action;
        Command::Restrictions restrictions;
        Command *(*createCommand)(QAbstractItemView *, KeyListController *);
    };
    std::vector<action_item> actions;
    std::vector<QAbstractItemView *> views;
    std::vector<Command *> commands;
    QPointer<QWidget> parentWidget;
    QPointer<TabWidget> tabWidget;
    QPointer<QAbstractItemView> currentView;
    QPointer<AbstractKeyListModel> flatModel, hierarchicalModel;
};

KeyListController::Private::Private(KeyListController *qq)
    : q(qq),
      actions(),
      views(),
      commands(),
      parentWidget(),
      tabWidget(),
      flatModel(),
      hierarchicalModel()
{
    connect(KeyCache::mutableInstance().get(), SIGNAL(added(GpgME::Key)),
            q, SLOT(slotAddKey(GpgME::Key)));
    connect(KeyCache::mutableInstance().get(), SIGNAL(aboutToRemove(GpgME::Key)),
            q, SLOT(slotAboutToRemoveKey(GpgME::Key)));
}

KeyListController::Private::~Private() {}

KeyListController::KeyListController(QObject *p)
    : QObject(p), d(new Private(this))
{

}

KeyListController::~KeyListController() {}

void KeyListController::Private::slotAddKey(const Key &key)
{
    // ### make model act on keycache directly...
    if (flatModel) {
        flatModel->addKey(key);
    }
    if (hierarchicalModel) {
        hierarchicalModel->addKey(key);
    }
}

void KeyListController::Private::slotAboutToRemoveKey(const Key &key)
{
    // ### make model act on keycache directly...
    if (flatModel) {
        flatModel->removeKey(key);
    }
    if (hierarchicalModel) {
        hierarchicalModel->removeKey(key);
    }
}

void KeyListController::addView(QAbstractItemView *view)
{
    if (!view || std::binary_search(d->views.cbegin(), d->views.cend(), view)) {
        return;
    }
    d->addView(view);
}

void KeyListController::removeView(QAbstractItemView *view)
{
    if (!view || !std::binary_search(d->views.cbegin(), d->views.cend(), view)) {
        return;
    }
    d->removeView(view);
}

void KeyListController::setCurrentView(QAbstractItemView *view)
{
    d->slotCurrentViewChanged(view);
}

std::vector<QAbstractItemView *> KeyListController::views() const
{
    return d->views;
}

void KeyListController::setFlatModel(AbstractKeyListModel *model)
{
    if (model == d->flatModel) {
        return;
    }

    d->flatModel = model;

    if (model) {
        model->clear();
        if (KeyCache::instance()->initialized()) {
            model->addKeys(KeyCache::instance()->keys());
        }
        model->setToolTipOptions(d->toolTipOptions());
    }
}

void KeyListController::setHierarchicalModel(AbstractKeyListModel *model)
{
    if (model == d->hierarchicalModel) {
        return;
    }

    d->hierarchicalModel = model;

    if (model) {
        model->clear();
        if (KeyCache::instance()->initialized()) {
            model->addKeys(KeyCache::instance()->keys());
        }
        model->setToolTipOptions(d->toolTipOptions());
    }
}

void KeyListController::setTabWidget(TabWidget *tabWidget)
{
    if (tabWidget == d->tabWidget) {
        return;
    }

    d->disconnectTabWidget();

    d->tabWidget = tabWidget;

    d->connectTabWidget();

    d->slotCurrentViewChanged(tabWidget ? tabWidget->currentView() : nullptr);
}

void KeyListController::setParentWidget(QWidget *parent)
{
    d->parentWidget = parent;
}

QWidget *KeyListController::parentWidget() const
{
    return d->parentWidget;
}

static const struct {
    const char *signal;
    const char *slot;
} tabs2controller[] = {
    { SIGNAL(viewAdded(QAbstractItemView*)),            SLOT(addView(QAbstractItemView*))                },
    { SIGNAL(viewAboutToBeRemoved(QAbstractItemView*)), SLOT(removeView(QAbstractItemView*))             },
    { SIGNAL(currentViewChanged(QAbstractItemView*)),   SLOT(slotCurrentViewChanged(QAbstractItemView*)) },
};
static const unsigned int numTabs2Controller = sizeof tabs2controller / sizeof * tabs2controller;

void KeyListController::Private::connectTabWidget()
{
    if (!tabWidget) {
        return;
    }
    const auto views = tabWidget->views();
    std::for_each(views.cbegin(), views.cend(),
                  [this](QAbstractItemView *view) { addView(view); });
    for (unsigned int i = 0; i < numTabs2Controller; ++i) {
        connect(tabWidget, tabs2controller[i].signal, q, tabs2controller[i].slot);
    }
}

void KeyListController::Private::disconnectTabWidget()
{
    if (!tabWidget) {
        return;
    }
    for (unsigned int i = 0; i < numTabs2Controller; ++i) {
        disconnect(tabWidget, tabs2controller[i].signal, q, tabs2controller[i].slot);
    }
    const auto views = tabWidget->views();
    std::for_each(views.cbegin(), views.cend(),
                  [this](QAbstractItemView *view) { removeView(view); });
}

AbstractKeyListModel *KeyListController::flatModel() const
{
    return d->flatModel;
}

AbstractKeyListModel *KeyListController::hierarchicalModel() const
{
    return d->hierarchicalModel;
}

QAbstractItemView *KeyListController::currentView() const
{
    return d->currentView;
}

TabWidget *KeyListController::tabWidget() const
{
    return d->tabWidget;
}

void KeyListController::createActions(KActionCollection *coll)
{

    const action_data action_data[] = {
        // File menu
        {
            "file_new_certificate", i18n("New Key Pair..."), QString(),
            "view-certificate-add", nullptr, nullptr, QStringLiteral("Ctrl+N"), false, true
        },
        {
            "file_export_certificates", i18n("Export..."), i18n("Export the selected certificate (public key) to a file"),
            "view-certificate-export", nullptr, nullptr, QStringLiteral("Ctrl+E"), false, true
        },
        {
            "file_export_certificates_to_server", i18n("Publish on Server..."), i18n("Publish the selected certificate (public key) on a public keyserver"),
            "view-certificate-export-server", nullptr, nullptr, QStringLiteral("Ctrl+Shift+E"), false, true
        },
        {
            "file_export_secret_keys", i18n("Export Secret Keys..."), QString(),
            "view-certificate-export-secret", nullptr, nullptr, QString(), false, true
        },
        {
            "file_export_paper_key", i18n("Print Secret Key..."), QString(),
            "document-print", nullptr, nullptr, QString(), false, true
        },
        {
            "file_lookup_certificates", i18n("Lookup on Server..."), i18n("Search for certificates online using a public keyserver"),
            "edit-find", nullptr, nullptr, QStringLiteral("Shift+Ctrl+I"), false, true
        },
        {
            "file_import_certificates", i18n("Import..."), i18n("Import a certificate from a file"),
            "view-certificate-import", nullptr, nullptr, QStringLiteral("Ctrl+I"), false, true
        },
        {
            "file_decrypt_verify_files", i18n("Decrypt/Verify..."), i18n("Decrypt and/or verify files"),
            "document-edit-decrypt-verify", nullptr, nullptr, QString(), false, true
        },
        {
            "file_sign_encrypt_files", i18n("Sign/Encrypt..."), i18n("Encrypt and/or sign files"),
            "document-edit-sign-encrypt", nullptr, nullptr, QString(), false, true
        },
        {
            "file_sign_encrypt_folder", i18n("Sign/Encrypt Folder..."), i18n("Encrypt and/or sign folders"),
            nullptr/*"folder-edit-sign-encrypt"*/, nullptr, nullptr, QString(), false, true
        },
        {
            "file_checksum_create_files", i18n("Create Checksum Files..."), QString(),
            nullptr/*"document-checksum-create"*/, nullptr, nullptr, QString(), false, true
        },
        {
            "file_checksum_verify_files", i18n("Verify Checksum Files..."), QString(),
            nullptr/*"document-checksum-verify"*/, nullptr, nullptr, QString(), false, true
        },
        // View menu
        {
            "view_redisplay", i18n("Redisplay"), QString(),
            "view-refresh", nullptr, nullptr, QStringLiteral("F5"), false, true
        },
        {
            "view_stop_operations", i18n("Stop Operation"), QString(),
            "process-stop", this, SLOT(cancelCommands()), QStringLiteral("Escape"), false, false
        },
        {
            "view_certificate_details", i18n("Details"), QString(),
            "dialog-information", nullptr, nullptr, QString(), false, true
        },
        // Certificate menu
        {
            "certificates_delete", i18n("Delete"), i18n("Delete selected certificates"),
            "edit-delete", nullptr, nullptr, QStringLiteral("Delete"), false, true
        },
        {
            "certificates_certify_certificate", i18n("Certify..."), i18n("Certify the validity of the selected certificate"),
            "view-certificate-sign", nullptr, nullptr, QString(), false, true
        },
        {
            "certificates_change_expiry", i18n("Change Expiry Date..."), QString(),
            nullptr, nullptr, nullptr, QString(), false, true
        },
        {
            "certificates_change_owner_trust", i18n("Change Certification Trust..."), QString(),
            nullptr, nullptr, nullptr, QString(), false, true
        },
        {
            "certificates_trust_root", i18n("Trust Root Certificate"), QString(),
            nullptr, nullptr, nullptr, QString(), false, true
        },
        {
            "certificates_distrust_root", i18n("Distrust Root Certificate"), QString(),
            nullptr, nullptr, nullptr, QString(), false, true
        },
        {
            "certificates_change_passphrase", i18n("Change Passphrase..."), QString(),
            nullptr, nullptr, nullptr, QString(), false, true
        },
        {
            "certificates_add_userid", i18n("Add User-ID..."), QString(),
            nullptr, nullptr, nullptr, QString(), false, true
        },
        {
            "certificates_dump_certificate", i18n("Technical Details"), QString(),
            nullptr, nullptr, nullptr, QString(), false, true
        },
        // Tools menu
        {
            "tools_refresh_x509_certificates", i18n("Refresh X.509 Certificates"), QString(),
            "view-refresh", nullptr, nullptr, QString(), false, true
        },
        {
            "tools_refresh_openpgp_certificates", i18n("Refresh OpenPGP Certificates"), QString(),
            "view-refresh", nullptr, nullptr, QString(), false, true
        },
        {
            "crl_clear_crl_cache", i18n("Clear CRL Cache"), QString(),
            nullptr, nullptr, nullptr, QString(), false, true
        },
        {
            "crl_dump_crl_cache", i18n("Dump CRL Cache"), QString(),
            nullptr, nullptr, nullptr, QString(), false, true
        },
        {
            "crl_import_crl", i18n("Import CRL From File..."), QString(),
            nullptr, nullptr, nullptr, QString(), false, true
        },
        // Window menu
        // (come from TabWidget)
        // Help menu
        // (come from MainWindow)
    };

    make_actions_from_data(action_data, coll);

    if (QAction *action = coll->action(QStringLiteral("view_stop_operations"))) {
        connect(this, &KeyListController::commandsExecuting, action, &QAction::setEnabled);
    }

    // ### somehow make this better...
    registerActionForCommand<NewCertificateCommand>(coll->action(QStringLiteral("file_new_certificate")));
    //---
    registerActionForCommand<LookupCertificatesCommand>(coll->action(QStringLiteral("file_lookup_certificates")));
    registerActionForCommand<ImportCertificateFromFileCommand>(coll->action(QStringLiteral("file_import_certificates")));
    //---
    registerActionForCommand<ExportCertificateCommand>(coll->action(QStringLiteral("file_export_certificates")));
    registerActionForCommand<ExportSecretKeyCommand>(coll->action(QStringLiteral("file_export_secret_keys")));
    registerActionForCommand<ExportPaperKeyCommand>(coll->action(QStringLiteral("file_export_paper_key")));
    registerActionForCommand<ExportOpenPGPCertsToServerCommand>(coll->action(QStringLiteral("file_export_certificates_to_server")));
    //---
    registerActionForCommand<DecryptVerifyFilesCommand>(coll->action(QStringLiteral("file_decrypt_verify_files")));
    registerActionForCommand<SignEncryptFilesCommand>(coll->action(QStringLiteral("file_sign_encrypt_files")));
    registerActionForCommand<SignEncryptFolderCommand>(coll->action(QStringLiteral("file_sign_encrypt_folder")));
    //---
    registerActionForCommand<ChecksumCreateFilesCommand>(coll->action(QStringLiteral("file_checksum_create_files")));
    registerActionForCommand<ChecksumVerifyFilesCommand>(coll->action(QStringLiteral("file_checksum_verify_files")));

    registerActionForCommand<ReloadKeysCommand>(coll->action(QStringLiteral("view_redisplay")));
    //coll->action( "view_stop_operations" ) <-- already dealt with in make_actions_from_data()
    registerActionForCommand<DetailsCommand>(coll->action(QStringLiteral("view_certificate_details")));

    registerActionForCommand<ChangeOwnerTrustCommand>(coll->action(QStringLiteral("certificates_change_owner_trust")));
    registerActionForCommand<TrustRootCommand>(coll->action(QStringLiteral("certificates_trust_root")));
    registerActionForCommand<DistrustRootCommand>(coll->action(QStringLiteral("certificates_distrust_root")));
    //---
    registerActionForCommand<CertifyCertificateCommand>(coll->action(QStringLiteral("certificates_certify_certificate")));
    registerActionForCommand<ChangeExpiryCommand>(coll->action(QStringLiteral("certificates_change_expiry")));
    registerActionForCommand<ChangePassphraseCommand>(coll->action(QStringLiteral("certificates_change_passphrase")));
    registerActionForCommand<AddUserIDCommand>(coll->action(QStringLiteral("certificates_add_userid")));
    //---
    registerActionForCommand<DeleteCertificatesCommand>(coll->action(QStringLiteral("certificates_delete")));
    //---
    registerActionForCommand<DumpCertificateCommand>(coll->action(QStringLiteral("certificates_dump_certificate")));

    registerActionForCommand<RefreshX509CertsCommand>(coll->action(QStringLiteral("tools_refresh_x509_certificates")));
    registerActionForCommand<RefreshOpenPGPCertsCommand>(coll->action(QStringLiteral("tools_refresh_openpgp_certificates")));
    //---
    registerActionForCommand<ImportCrlCommand>(coll->action(QStringLiteral("crl_import_crl")));
    //---
    registerActionForCommand<ClearCrlCacheCommand>(coll->action(QStringLiteral("crl_clear_crl_cache")));
    registerActionForCommand<DumpCrlCacheCommand>(coll->action(QStringLiteral("crl_dump_crl_cache")));

    enableDisableActions(nullptr);
}

void KeyListController::registerAction(QAction *action, Command::Restrictions restrictions, Command * (*create)(QAbstractItemView *, KeyListController *))
{
    if (!action) {
        return;
    }
    Q_ASSERT(!action->isCheckable());   // can be added later, for now, disallow

    const Private::action_item ai = {
        action, restrictions, create
    };
    connect(action, SIGNAL(triggered()), this, SLOT(slotActionTriggered()));
    d->actions.push_back(ai);
}

void KeyListController::registerCommand(Command *cmd)
{
    if (!cmd || std::binary_search(d->commands.cbegin(), d->commands.cend(), cmd)) {
        return;
    }
    d->addCommand(cmd);
    qCDebug(KLEOPATRA_LOG) << (void *)cmd;
    if (d->commands.size() == 1) {
        Q_EMIT commandsExecuting(true);
    }
}

bool KeyListController::hasRunningCommands() const
{
    return !d->commands.empty();
}

bool KeyListController::shutdownWarningRequired() const
{
    return std::any_of(d->commands.cbegin(), d->commands.cend(), std::mem_fn(&Command::warnWhenRunningAtShutdown));
}

// slot
void KeyListController::cancelCommands()
{
    std::for_each(d->commands.begin(), d->commands.end(), std::mem_fn(&Command::cancel));
}

void KeyListController::Private::connectView(QAbstractItemView *view)
{

    connect(view, SIGNAL(destroyed(QObject*)),
            q, SLOT(slotDestroyed(QObject*)));
    connect(view, SIGNAL(doubleClicked(QModelIndex)),
            q, SLOT(slotDoubleClicked(QModelIndex)));
    connect(view, SIGNAL(activated(QModelIndex)),
            q, SLOT(slotActivated(QModelIndex)));
    connect(view->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            q, SLOT(slotSelectionChanged(QItemSelection,QItemSelection)));

    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view, SIGNAL(customContextMenuRequested(QPoint)),
            q, SLOT(slotContextMenu(QPoint)));
}

void KeyListController::Private::connectCommand(Command *cmd)
{
    if (!cmd) {
        return;
    }
    connect(cmd, SIGNAL(destroyed(QObject*)), q, SLOT(slotDestroyed(QObject*)));
    connect(cmd, SIGNAL(finished()), q, SLOT(slotCommandFinished()));
    //connect( cmd, SIGNAL(canceled()), q, SLOT(slotCommandCanceled()) );
    connect(cmd, &Command::info, q, &KeyListController::message);
    connect(cmd, SIGNAL(progress(QString,int,int)), q, SLOT(slotProgress(QString,int,int)));
}

void KeyListController::Private::slotDoubleClicked(const QModelIndex &idx)
{
    QAbstractItemView *const view = qobject_cast<QAbstractItemView *>(q->sender());
    if (!view || !std::binary_search(views.cbegin(), views.cend(), view)) {
        return;
    }

    DetailsCommand *const c = new DetailsCommand(view, q);
    if (parentWidget) {
        c->setParentWidget(parentWidget);
    }

    c->setIndex(idx);
    c->start();
}

void KeyListController::Private::slotActivated(const QModelIndex &idx)
{
    Q_UNUSED(idx);
    QAbstractItemView *const view = qobject_cast<QAbstractItemView *>(q->sender());
    if (!view || !std::binary_search(views.cbegin(), views.cend(), view)) {
        return;
    }

}

void KeyListController::Private::slotSelectionChanged(const QItemSelection &old, const QItemSelection &new_)
{
    Q_UNUSED(old);
    Q_UNUSED(new_);

    const QItemSelectionModel *const sm = qobject_cast<QItemSelectionModel *>(q->sender());
    if (!sm) {
        return;
    }
    q->enableDisableActions(sm);
}

void KeyListController::Private::slotContextMenu(const QPoint &p)
{
    QAbstractItemView *const view = qobject_cast<QAbstractItemView *>(q->sender());
    if (view && std::binary_search(views.cbegin(), views.cend(), view)) {
        Q_EMIT q->contextMenuRequested(view, view->viewport()->mapToGlobal(p));
    } else {
        qCDebug(KLEOPATRA_LOG) << "sender is not a QAbstractItemView*!";
    }
}

void KeyListController::Private::slotCommandFinished()
{
    Command *const cmd = qobject_cast<Command *>(q->sender());
    if (!cmd || !std::binary_search(commands.cbegin(), commands.cend(), cmd)) {
        return;
    }
    qCDebug(KLEOPATRA_LOG) << (void *)cmd;
    if (commands.size() == 1) {
        Q_EMIT q->commandsExecuting(false);
    }
}

void KeyListController::enableDisableActions(const QItemSelectionModel *sm) const
{
    const Command::Restrictions restrictionsMask = d->calculateRestrictionsMask(sm);
    Q_FOREACH (const Private::action_item &ai, d->actions)
        if (ai.action) {
            ai.action->setEnabled(ai.restrictions == (ai.restrictions & restrictionsMask));
        }
}

static bool all_secret_are_not_owner_trust_ultimate(const std::vector<Key> &keys)
{
    for (const Key &key : keys)
        if (key.hasSecret() && key.ownerTrust() == Key::Ultimate) {
            return false;
        }
    return true;
}

Command::Restrictions find_root_restrictions(const std::vector<Key> &keys)
{
    bool trusted = false, untrusted = false;
    for (const Key &key : keys)
        if (key.isRoot())
            if (key.userID(0).validity() == UserID::Ultimate) {
                trusted = true;
            } else {
                untrusted = true;
            }
        else {
            return Command::NoRestriction;
        }
    if (trusted)
        if (untrusted) {
            return Command::NoRestriction;
        } else {
            return Command::MustBeTrustedRoot;
        }
    else if (untrusted) {
        return Command::MustBeUntrustedRoot;
    } else {
        return Command::NoRestriction;
    }
}

Command::Restrictions KeyListController::Private::calculateRestrictionsMask(const QItemSelectionModel *sm)
{
    if (!sm) {
        return Command::NoRestriction;
    }

    const KeyListModelInterface *const m = dynamic_cast<const KeyListModelInterface *>(sm->model());
    if (!m) {
        return Command::NoRestriction;
    }

    const std::vector<Key> keys = m->keys(sm->selectedRows());
    if (keys.empty()) {
        return Command::NoRestriction;
    }

    Command::Restrictions result = Command::NeedSelection;

    if (keys.size() == 1) {
        result |= Command::OnlyOneKey;
    }

    if (std::all_of(keys.cbegin(), keys.cend(), std::mem_fn(&Key::hasSecret))) {
        result |= Command::NeedSecretKey;
    } else if (!std::any_of(keys.cbegin(), keys.cend(), std::mem_fn(&Key::hasSecret))) {
        result |= Command::MustNotBeSecretKey;
    }

    if (std::all_of(keys.cbegin(), keys.cend(), [](const Key &key) { return key.protocol() == OpenPGP; })) {
        result |= Command::MustBeOpenPGP;
    } else if (std::all_of(keys.cbegin(), keys.cend(), [](const Key &key) { return key.protocol() == CMS; })) {
        result |= Command::MustBeCMS;
    }

    if (all_secret_are_not_owner_trust_ultimate(keys)) {
        result |= Command::MayOnlyBeSecretKeyIfOwnerTrustIsNotYetUltimate;
    }

    result |= find_root_restrictions(keys);

    if (const ReaderStatus *rs = ReaderStatus::instance()) {
        if (rs->anyCardHasNullPin()) {
            result |= Command::AnyCardHasNullPin;
        }
        if (rs->anyCardCanLearnKeys()) {
            result |= Command::AnyCardCanLearnKeys;
        }
    }

    return result;
}

void KeyListController::Private::slotActionTriggered()
{
    if (const QObject *const s = q->sender()) {
        const auto it = std::find_if(actions.cbegin(), actions.cend(),
                                     [this](const action_item &item) { return item.action == q->sender(); });
        if (it != actions.end())
            if (Command *const c = it->createCommand(this->currentView, q)) {
                if (parentWidget) {
                    c->setParentWidget(parentWidget);
                }
                c->start();
            } else
                qCDebug(KLEOPATRA_LOG) << "createCommand() == NULL for action(?) \""
                                       << qPrintable(s->objectName()) << "\"";
        else {
            qCDebug(KLEOPATRA_LOG) << "I don't know anything about action(?) \"%s\"", qPrintable(s->objectName());
        }
    } else {
        qCDebug(KLEOPATRA_LOG) << "not called through a signal/slot connection (sender() == NULL)";
    }
}

int KeyListController::Private::toolTipOptions() const
{
    using namespace Kleo::Formatting;
    static const int validityFlags = Validity | Issuer | ExpiryDates | CertificateUsage;
    static const int ownerFlags = Subject | UserIDs | OwnerTrust;
    static const int detailsFlags = StorageLocation | CertificateType | SerialNumber | Fingerprint;

    const TooltipPreferences prefs;

    int flags = KeyID;
    flags |= prefs.showValidity() ? validityFlags : 0;
    flags |= prefs.showOwnerInformation() ? ownerFlags : 0;
    flags |= prefs.showCertificateDetails() ? detailsFlags : 0;
    return flags;
}

void KeyListController::updateConfig()
{
    const int opts = d->toolTipOptions();
    if (d->flatModel) {
        d->flatModel->setToolTipOptions(opts);
    }
    if (d->hierarchicalModel) {
        d->hierarchicalModel->setToolTipOptions(opts);
    }
}

#include "moc_keylistcontroller.cpp"
