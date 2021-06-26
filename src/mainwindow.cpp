/* -*- mode: c++; c-basic-offset:4 -*-
    mainwindow.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>
#include "mainwindow.h"
#include "aboutdata.h"
#include "settings.h"

#include "view/padwidget.h"
#include "view/searchbar.h"
#include "view/tabwidget.h"
#include "view/keylistcontroller.h"
#include "view/keycacheoverlay.h"
#include "view/smartcardwidget.h"
#include "view/welcomewidget.h"

#include "commands/selftestcommand.h"
#include "commands/importcrlcommand.h"
#include "commands/importcertificatefromfilecommand.h"
#include "commands/decryptverifyfilescommand.h"
#include "commands/signencryptfilescommand.h"

#include "conf/groupsconfigdialog.h"

#include "utils/detail_p.h"
#include <Libkleo/GnuPG>
#include "utils/action_data.h"
#include "utils/filedialog.h"
#include "utils/clipboardmenu.h"

#include "dialogs/updatenotification.h"

#include <KXMLGUIFactory>
#include <QApplication>
#include <QSize>
#include <QLineEdit>
#include <KActionMenu>
#include <KActionCollection>
#include <KLocalizedString>
#include <KStandardAction>
#include <QAction>
#include <KAboutData>
#include <KMessageBox>
#include <KStandardGuiItem>
#include <KShortcutsDialog>
#include <KEditToolBar>
#include "kleopatra_debug.h"
#include <KConfigGroup>
#include <KConfigDialog>
#include <kxmlgui_version.h>

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QMenu>
#include <QTimer>
#include <QProcess>
#include <QVBoxLayout>
#include <QMimeData>
#include <QDesktopServices>
#include <QDir>
#include <QStackedWidget>
#include <QStatusBar>
#include <QLabel>

#include <Libkleo/Formatting>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeyListSortFilterProxyModel>
#include <Libkleo/Stl_Util>
#include <Libkleo/Classify>
#include <Libkleo/KeyCache>

#include <vector>
#include <KSharedConfig>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

static KGuiItem KStandardGuiItem_quit()
{
    static const QString app = KAboutData::applicationData().componentName();
    KGuiItem item = KStandardGuiItem::quit();
    item.setText(i18nc("Quit [ApplicationName]", "&Quit %1", app));
    return item;
}

static KGuiItem KStandardGuiItem_close()
{
    KGuiItem item = KStandardGuiItem::close();
    item.setText(i18n("Only &Close Window"));
    return item;
}

static bool isQuitting = false;

namespace
{
static const std::vector<QString> mainViewActionNames = {
    QStringLiteral("view_certificate_overview"),
    QStringLiteral("manage_smartcard"),
    QStringLiteral("pad_view")
};
}

class MainWindow::Private
{
    friend class ::MainWindow;
    MainWindow *const q;

public:
    explicit Private(MainWindow *qq);
    ~Private();

    template <typename T>
    void createAndStart()
    {
        (new T(this->currentView(), &this->controller))->start();
    }
    template <typename T>
    void createAndStart(QAbstractItemView *view)
    {
        (new T(view, &this->controller))->start();
    }
    template <typename T>
    void createAndStart(const QStringList &a)
    {
        (new T(a, this->currentView(), &this->controller))->start();
    }
    template <typename T>
    void createAndStart(const QStringList &a, QAbstractItemView *view)
    {
        (new T(a, view, &this->controller))->start();
    }

    void closeAndQuit()
    {
        const QString app = KAboutData::applicationData().componentName();
        const int rc = KMessageBox::questionYesNoCancel(q,
                       i18n("%1 may be used by other applications as a service.\n"
                            "You may instead want to close this window without exiting %1.", app),
                       i18n("Really Quit?"), KStandardGuiItem_close(), KStandardGuiItem_quit(), KStandardGuiItem::cancel(),
                       QLatin1String("really-quit-") + app.toLower());
        if (rc == KMessageBox::Cancel) {
            return;
        }
        isQuitting = true;
        if (!q->close()) {
            return;
        }
        // WARNING: 'this' might be deleted at this point!
        if (rc == KMessageBox::No) {
            qApp->quit();
        }
    }
    void configureToolbars()
    {
        KEditToolBar dlg(q->factory());
        dlg.exec();
    }
    void editKeybindings()
    {
#if KXMLGUI_VERSION < QT_VERSION_CHECK(5,84,0)
        KShortcutsDialog::configure(q->actionCollection(), KShortcutsEditor::LetterShortcutsAllowed);
#else
        KShortcutsDialog::showDialog(q->actionCollection(),  KShortcutsEditor::LetterShortcutsAllowed, q);
#endif
        updateSearchBarClickMessage();
    }

    void updateSearchBarClickMessage()
    {
        const QString shortcutStr = focusToClickSearchAction->shortcut().toString();
        ui.searchBar->updateClickMessage(shortcutStr);
    }

    void updateStatusBar()
    {
        const auto complianceMode = Formatting::complianceMode();
        if (complianceMode == QStringLiteral("de-vs")) {
            auto statusBar = new QStatusBar;
            q->setStatusBar(statusBar);
            auto statusLbl = new QLabel(Formatting::deVsString());
            statusBar->insertPermanentWidget(0, statusLbl);

        } else {
            q->setStatusBar(nullptr);
        }
    }

    void selfTest()
    {
        createAndStart<SelfTestCommand>();
    }

    void configureGroups()
    {
        if (KConfigDialog::showDialog(GroupsConfigDialog::dialogName())) {
            return;
        }
        KConfigDialog *dialog = new GroupsConfigDialog(q);
        dialog->show();
    }

    void showHandbook();

    void gnupgLogViewer()
    {
        if (!QProcess::startDetached(QStringLiteral("kwatchgnupg"), QStringList()))
            KMessageBox::error(q, i18n("Could not start the GnuPG Log Viewer (kwatchgnupg). "
                                       "Please check your installation."),
                               i18n("Error Starting KWatchGnuPG"));
    }

    void forceUpdateCheck()
    {
        UpdateNotification::forceUpdateCheck(q);
    }

    void openCompendium()
    {
        QDir datadir(QCoreApplication::applicationDirPath() + QStringLiteral("/../share/gpg4win"));
        const auto path = datadir.filePath(i18nc("The Gpg4win compendium is only available"
                                                 "at this point (24.7.2017) in german and english."
                                                 "Please check with Gpg4win before translating this filename.",
                                                 "gpg4win-compendium-en.pdf"));
        qCDebug(KLEOPATRA_LOG) << "Opening Compendium at:" << path;
        // The compendium is always installed. So this should work. Otherwise
        // we have debug output.
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }

    void slotConfigCommitted();
    void slotContextMenuRequested(QAbstractItemView *, const QPoint &p)
    {
        if (auto const menu = qobject_cast<QMenu *>(q->factory()->container(QStringLiteral("listview_popup"), q))) {
            menu->exec(p);
        } else {
            qCDebug(KLEOPATRA_LOG) << "no \"listview_popup\" <Menu> in kleopatra's ui.rc file";
        }
    }

    void slotFocusQuickSearch()
    {
        ui.searchBar->lineEdit()->setFocus();
    }

    void showView(const QString &actionName, QWidget *widget)
    {
        const auto coll = q->actionCollection();
        if (coll) {
            for ( const QString &name : mainViewActionNames ) {
                if (auto action = coll->action(name)) {
                    action->setChecked(name == actionName);
                }
            }
        }
        ui.stackWidget->setCurrentWidget(widget);
    }

    void showCertificateView()
    {
        showView(QStringLiteral("view_certificate_overview"),
                 KeyCache::instance()->keys().empty() ? ui.welcomeWidget : ui.searchTab);
    }

    void showSmartcardView()
    {
        showView(QStringLiteral("manage_smartcard"), ui.scWidget);
    }

    void showPadView()
    {
        if (!ui.padWidget) {
            ui.padWidget = new PadWidget;
            ui.stackWidget->addWidget(ui.padWidget);
        }
        showView(QStringLiteral("pad_view"), ui.padWidget);
        ui.stackWidget->resize(ui.padWidget->sizeHint());
    }

private:
    void setupActions();

    QAbstractItemView *currentView() const
    {
        return ui.tabWidget.currentView();
    }

    void keyListingDone()
    {
        const auto curWidget = ui.stackWidget->currentWidget();
        if (curWidget == ui.scWidget || curWidget == ui.padWidget) {
           return;
        }
        showCertificateView();
    }

private:
    Kleo::KeyListController controller;
    bool firstShow : 1;
    struct UI {
        QWidget *searchTab;
        TabWidget tabWidget;
        SearchBar *searchBar;
        PadWidget *padWidget;
        SmartCardWidget *scWidget;
        WelcomeWidget *welcomeWidget;
        QStackedWidget *stackWidget;
        explicit UI(MainWindow *q);
    } ui;
    QAction *focusToClickSearchAction;
    ClipboardMenu *clipboadMenu;
};

MainWindow::Private::UI::UI(MainWindow *q)
    : tabWidget(q), padWidget(nullptr)
{
    KDAB_SET_OBJECT_NAME(tabWidget);

    searchTab = new QWidget;
    auto vbox = new QVBoxLayout(searchTab);
    vbox->setSpacing(0);
    searchBar = new SearchBar;
    vbox->addWidget(searchBar);
    tabWidget.connectSearchBar(searchBar);
    vbox->addWidget(&tabWidget);

    auto mainWidget = new QWidget;
    auto mainLayout = new QVBoxLayout(mainWidget);
    stackWidget = new QStackedWidget;

    mainLayout->addWidget(stackWidget);

    stackWidget->addWidget(searchTab);

    new KeyCacheOverlay(mainWidget, q);

    scWidget = new SmartCardWidget();
    stackWidget->addWidget(scWidget);

    welcomeWidget = new WelcomeWidget();
    stackWidget->addWidget(welcomeWidget);

    q->setCentralWidget(mainWidget);
}

MainWindow::Private::Private(MainWindow *qq)
    : q(qq),
      controller(q),
      firstShow(true),
      ui(q)
{
    KDAB_SET_OBJECT_NAME(controller);

    AbstractKeyListModel *flatModel = AbstractKeyListModel::createFlatKeyListModel(q);
    AbstractKeyListModel *hierarchicalModel = AbstractKeyListModel::createHierarchicalKeyListModel(q);

    KDAB_SET_OBJECT_NAME(flatModel);
    KDAB_SET_OBJECT_NAME(hierarchicalModel);

    controller.setFlatModel(flatModel);
    controller.setHierarchicalModel(hierarchicalModel);
    controller.setTabWidget(&ui.tabWidget);

    ui.tabWidget.setFlatModel(flatModel);
    ui.tabWidget.setHierarchicalModel(hierarchicalModel);

    setupActions();

    ui.stackWidget->setCurrentWidget(ui.searchTab);
    if (auto action = q->actionCollection()->action(QStringLiteral("view_certificate_overview"))) {
        action->setChecked(true);
    }

    connect(&controller, SIGNAL(contextMenuRequested(QAbstractItemView*,QPoint)), q, SLOT(slotContextMenuRequested(QAbstractItemView*,QPoint)));
    connect(KeyCache::instance().get(), &KeyCache::keyListingDone, q, [this] () {keyListingDone();});

    q->createGUI(QStringLiteral("kleopatra.rc"));

    q->setAcceptDrops(true);

    // set default window size
    q->resize(QSize(1024, 500));
    q->setAutoSaveSettings();

    updateSearchBarClickMessage();
    updateStatusBar();
}

MainWindow::Private::~Private() {}

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
    : KXmlGuiWindow(parent, flags), d(new Private(this))
{}

MainWindow::~MainWindow() {}

void MainWindow::Private::setupActions()
{

    KActionCollection *const coll = q->actionCollection();

    const action_data action_data[] = {
        // most have been MOVED TO keylistcontroller.cpp
        // Tools menu
#ifndef Q_OS_WIN
        {
            "tools_start_kwatchgnupg", i18n("GnuPG Log Viewer"), QString(),
            "kwatchgnupg", q, SLOT(gnupgLogViewer()), QString(), false, true
        },
#endif
#ifdef Q_OS_WIN
        {
            "help_check_updates", i18n("Check for updates"), QString(),
            "gpg4win-compact", q, SLOT(forceUpdateCheck()), QString(), false, true
        },
        {
            "help_show_compendium", i18n("Gpg4win Compendium"), QString(),
            "gpg4win-compact", q, SLOT(openCompendium()), QString(), false, true
        },
#endif
        {
            "view_certificate_overview", i18nc("@action show certificate overview", "Certificates"),
            i18n("Show certificate overview"), "view-certificate", q, SLOT(showCertificateView()), QString(), false, true
        },
        {
            "pad_view", i18nc("@action show input / output area for encrypting/signing resp. decrypting/verifying text", "Notepad"),
            i18n("Show pad for encrypting/decrypting and signing/verifying text"), "note", q, SLOT(showPadView()), QString(), false, true
        },
        // most have been MOVED TO keylistcontroller.cpp
        // Settings menu
        {
            "settings_self_test", i18n("Perform Self-Test"), QString(),
            nullptr, q, SLOT(selfTest()), QString(), false, true
        },
        {
            "configure_groups", i18n("Configure Groups..."), QString(),
            "group", q, SLOT(configureGroups()), QString(), false, true
        },
        {
            "manage_smartcard", i18nc("@action show smartcard management view", "Smartcards"),
            i18n("Show smartcard management"), "auth-sim-locked", q, SLOT(showSmartcardView()), QString(), false, true
        }

        // most have been MOVED TO keylistcontroller.cpp
    };

    make_actions_from_data(action_data, /*sizeof action_data / sizeof *action_data,*/ coll);

    if (!Settings().groupsEnabled()) {
        if (auto action = coll->action(QStringLiteral("configure_groups"))) {
            delete action;
        }
    }

    for ( const QString &name : mainViewActionNames ) {
        if (auto action = coll->action(name)) {
            action->setCheckable(true);
        }
    }

    if (QAction *action = coll->action(QStringLiteral("configure_backend"))) {
        action->setMenuRole(QAction::NoRole);    //prevent Qt OS X heuristics for config* actions
    }

    KStandardAction::close(q, SLOT(close()), coll);
    KStandardAction::quit(q, SLOT(closeAndQuit()), coll);
    KStandardAction::configureToolbars(q, SLOT(configureToolbars()), coll);
    KStandardAction::keyBindings(q, SLOT(editKeybindings()), coll);
    KStandardAction::preferences(qApp, SLOT(openOrRaiseConfigDialog()), coll);

    focusToClickSearchAction = new QAction(i18n("Set Focus to Quick Search"), q);
    coll->addAction(QStringLiteral("focus_to_quickseach"), focusToClickSearchAction);
    coll->setDefaultShortcut(focusToClickSearchAction, QKeySequence(Qt::ALT | Qt::Key_Q));
    connect(focusToClickSearchAction, SIGNAL(triggered(bool)), q, SLOT(slotFocusQuickSearch()));
    clipboadMenu = new ClipboardMenu(q);
    clipboadMenu->setMainWindow(q);
    clipboadMenu->clipboardMenu()->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
    clipboadMenu->clipboardMenu()->setPopupMode(QToolButton::InstantPopup);
    coll->addAction(QStringLiteral("clipboard_menu"), clipboadMenu->clipboardMenu());

    q->setStandardToolBarMenuEnabled(true);

    controller.createActions(coll);

    ui.tabWidget.createActions(coll);
}

void MainWindow::Private::slotConfigCommitted()
{
    controller.updateConfig();
    updateStatusBar();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    // KMainWindow::closeEvent() insists on quitting the application,
    // so do not let it touch the event...
    qCDebug(KLEOPATRA_LOG);
    if (d->controller.hasRunningCommands()) {
        if (d->controller.shutdownWarningRequired()) {
            const int ret = KMessageBox::warningContinueCancel(this, i18n("There are still some background operations ongoing. "
                            "These will be terminated when closing the window. "
                            "Proceed?"),
                            i18n("Ongoing Background Tasks"));
            if (ret != KMessageBox::Continue) {
                e->ignore();
                return;
            }
        }
        d->controller.cancelCommands();
        if (d->controller.hasRunningCommands()) {
            // wait for them to be finished:
            setEnabled(false);
            QEventLoop ev;
            QTimer::singleShot(100, &ev, &QEventLoop::quit);
            connect(&d->controller, &KeyListController::commandsExecuting, &ev, &QEventLoop::quit);
            ev.exec();
            if (d->controller.hasRunningCommands())
                qCWarning(KLEOPATRA_LOG)
                        << "controller still has commands running, this may crash now...";
            setEnabled(true);
        }
    }
    if (isQuitting || qApp->isSavingSession()) {
        d->ui.tabWidget.saveViews(KSharedConfig::openConfig().data());
        KConfigGroup grp(KConfigGroup(KSharedConfig::openConfig(), autoSaveGroup()));
        saveMainWindowSettings(grp);
        e->accept();
    } else {
        e->ignore();
        hide();
    }
}

void MainWindow::showEvent(QShowEvent *e)
{
    KXmlGuiWindow::showEvent(e);
    if (d->firstShow) {
        d->ui.tabWidget.loadViews(KSharedConfig::openConfig().data());
        d->firstShow = false;
    }

    if (!savedGeometry.isEmpty()) {
        restoreGeometry(savedGeometry);
    }

}

void MainWindow::hideEvent(QHideEvent *e)
{
    savedGeometry = saveGeometry();
    KXmlGuiWindow::hideEvent(e);
}

void MainWindow::importCertificatesFromFile(const QStringList &files)
{
    if (!files.empty()) {
        d->createAndStart<ImportCertificateFromFileCommand>(files);
    }
}

static QStringList extract_local_files(const QMimeData *data)
{
    const QList<QUrl> urls = data->urls();
    // begin workaround KDE/Qt misinterpretation of text/uri-list
    QList<QUrl>::const_iterator end = urls.end();
    if (urls.size() > 1 && !urls.back().isValid()) {
        --end;
    }
    // end workaround
    QStringList result;
    std::transform(urls.begin(), end,
                   std::back_inserter(result),
                   std::mem_fn(&QUrl::toLocalFile));
    result.erase(std::remove_if(result.begin(), result.end(),
                                std::mem_fn(&QString::isEmpty)), result.end());
    return result;
}

static bool can_decode_local_files(const QMimeData *data)
{
    if (!data) {
        return false;
    }
    return !extract_local_files(data).empty();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    qCDebug(KLEOPATRA_LOG);

    if (can_decode_local_files(e->mimeData())) {
        e->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *e)
{
    qCDebug(KLEOPATRA_LOG);

    if (!can_decode_local_files(e->mimeData())) {
        return;
    }

    e->setDropAction(Qt::CopyAction);

    const QStringList files = extract_local_files(e->mimeData());

    const unsigned int classification = classify(files);

    QMenu menu;

    QAction *const signEncrypt = menu.addAction(i18n("Sign/Encrypt..."));
    QAction *const decryptVerify = mayBeAnyMessageType(classification) ? menu.addAction(i18n("Decrypt/Verify...")) : nullptr;
    if (signEncrypt || decryptVerify) {
        menu.addSeparator();
    }

    QAction *const importCerts = mayBeAnyCertStoreType(classification) ? menu.addAction(i18n("Import Certificates")) : nullptr;
    QAction *const importCRLs  = mayBeCertificateRevocationList(classification) ? menu.addAction(i18n("Import CRLs")) : nullptr;
    if (importCerts || importCRLs) {
        menu.addSeparator();
    }

    if (!signEncrypt && !decryptVerify && !importCerts && !importCRLs) {
        return;
    }

    menu.addAction(i18n("Cancel"));

    const QAction *const chosen = menu.exec(mapToGlobal(e->pos()));

    if (!chosen) {
        return;
    }

    if (chosen == signEncrypt) {
        d->createAndStart<SignEncryptFilesCommand>(files);
    } else if (chosen == decryptVerify) {
        d->createAndStart<DecryptVerifyFilesCommand>(files);
    } else if (chosen == importCerts) {
        d->createAndStart<ImportCertificateFromFileCommand>(files);
    } else if (chosen == importCRLs) {
        d->createAndStart<ImportCrlCommand>(files);
    }

    e->accept();
}

void MainWindow::readProperties(const KConfigGroup &cg)
{
    qCDebug(KLEOPATRA_LOG);
    KXmlGuiWindow::readProperties(cg);
    setHidden(cg.readEntry("hidden", false));
}

void MainWindow::saveProperties(KConfigGroup &cg)
{
    qCDebug(KLEOPATRA_LOG);
    KXmlGuiWindow::saveProperties(cg);
    cg.writeEntry("hidden", isHidden());
}

#include "moc_mainwindow.cpp"
