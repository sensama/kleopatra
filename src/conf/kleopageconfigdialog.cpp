/*
    kleopageconfigdialog.cpp

    This file is part of Kleopatra
    Copyright (c) 2016 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

    Kleopatra is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License,
    version 2, as published by the Free Software Foundation.

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

    It is derived from KCMultidialog which is:

    Copyright (c) 2000 Matthias Elter <elter@kde.org>
    Copyright (c) 2003 Daniel Molkentin <molkentin@kde.org>
    Copyright (c) 2003,2006 Matthias Kretz <kretz@kde.org>
    Copyright (c) 2004 Frans Englich <frans.englich@telia.com>
    Copyright (c) 2006 Tobias Koenig <tokoe@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <config-kleopatra.h>

#include "kleopageconfigdialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QLibrary>
#include <QCoreApplication>
#include <QUrl>
#include <QDesktopServices>
#include <QProcess>
#include <QFile>

#include <KCModule>
#include <KDesktopFile>
#include <KPluginLoader>
#include <KStandardGuiItem>
#include <KMessageBox>
#include <KLocalizedString>

#include "kleopatra_debug.h"

#define KCM_LIBRARY_NAME "kcm_kleopatra"

KleoPageConfigDialog::KleoPageConfigDialog(QWidget *parent)
    : KPageDialog(parent)
{
    setModal(false);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    buttonBox->setStandardButtons(QDialogButtonBox::Help
                                  | QDialogButtonBox::RestoreDefaults
                                  | QDialogButtonBox::Cancel
                                  | QDialogButtonBox::Apply
                                  | QDialogButtonBox::Ok
                                  | QDialogButtonBox::Reset);
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Ok), KStandardGuiItem::ok());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::RestoreDefaults),
                                       KStandardGuiItem::defaults());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Apply), KStandardGuiItem::apply());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Reset), KStandardGuiItem::reset());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Help), KStandardGuiItem::help());
    buttonBox->button(QDialogButtonBox::Reset)->setEnabled(false);
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked,
            this, &KleoPageConfigDialog::slotApplyClicked);
    connect(buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::clicked,
            this, &KleoPageConfigDialog::slotOkClicked);
    connect(buttonBox->button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked,
            this, &KleoPageConfigDialog::slotDefaultClicked);
    connect(buttonBox->button(QDialogButtonBox::Help), &QAbstractButton::clicked,
            this, &KleoPageConfigDialog::slotHelpClicked);
    connect(buttonBox->button(QDialogButtonBox::Reset), &QAbstractButton::clicked,
            this, &KleoPageConfigDialog::slotUser1Clicked);

    setButtonBox(buttonBox);

    connect(this, &KPageDialog::currentPageChanged,
            this, &KleoPageConfigDialog::slotCurrentPageChanged);
}

void KleoPageConfigDialog::slotCurrentPageChanged(KPageWidgetItem *current, KPageWidgetItem *previous)
{
    if (!previous) {
        return;
    }
    blockSignals(true);
    setCurrentPage(previous);

    KCModule *previousModule = qobject_cast<KCModule*>(previous->widget());
    bool canceled = false;
    if (previousModule && mChangedModules.contains(previousModule)) {
        const int queryUser = KMessageBox::warningYesNoCancel(
                          this,
                          i18n("The settings of the current module have changed.\n"
                               "Do you want to apply the changes or discard them?"),
                          i18n("Apply Settings"),
                          KStandardGuiItem::apply(),
                          KStandardGuiItem::discard(),
                          KStandardGuiItem::cancel());
        if (queryUser == KMessageBox::Yes) {
            previousModule->save();
        } else if (queryUser == KMessageBox::No) {
            previousModule->load();
        }
        canceled = queryUser == KMessageBox::Cancel;
    }
    if (!canceled) {
        mChangedModules.removeAll(previousModule);
        setCurrentPage(current);
    }
    blockSignals(false);

    clientChanged();
}

void KleoPageConfigDialog::apply()
{
    QPushButton *applyButton = buttonBox()->button(QDialogButtonBox::Apply);
    applyButton->setFocus();
    foreach (KCModule *module, mChangedModules) {
        module->save();
    }
    mChangedModules.clear();
    Q_EMIT configCommitted();
    clientChanged();
}

void KleoPageConfigDialog::slotDefaultClicked()
{
    const KPageWidgetItem *item = currentPage();
    if (!item) {
        return;
    }

    KCModule *module = qobject_cast<KCModule*>(item->widget());
    if (!module) {
        return;
    }
    module->defaults();
    clientChanged();
}

void KleoPageConfigDialog::slotUser1Clicked()
{
    const KPageWidgetItem *item = currentPage();
    if (!item) {
        return;
    }

    KCModule *module = qobject_cast<KCModule*>(item->widget());
    if (!module) {
        return;
    }
    module->load();
    mChangedModules.removeAll(module);
    clientChanged();
}

void KleoPageConfigDialog::slotApplyClicked()
{
    apply();
}

void KleoPageConfigDialog::slotOkClicked()
{
    apply();
    accept();
}

void KleoPageConfigDialog::slotHelpClicked()
{
    const KPageWidgetItem *item = currentPage();
    if (!item) {
        return;
    }

    const QString docPath = mHelpUrls.value(item->name());
    QUrl docUrl;

#ifdef Q_OS_WIN
    docUrl = QUrl(QLatin1String("https://docs.kde.org/index.php?branch=stable5&language=")
                  + QLocale().name() + QLatin1String("&application=kleopatra"));
#else
    docUrl = QUrl(QStringLiteral("help:/")).resolved(QUrl(docPath)); // same code as in KHelpClient::invokeHelp
#endif
    if (docUrl.scheme() == QLatin1String("help") || docUrl.scheme() == QLatin1String("man") || docUrl.scheme() == QLatin1String("info")) {
        QProcess::startDetached(QStringLiteral("khelpcenter"), QStringList() << docUrl.toString());
    } else {
        QDesktopServices::openUrl(docUrl);
    }
}

static KCModule *loadModule(const QString &name)
{
    QLibrary lib(KPluginLoader::findPlugin(QStringLiteral(KCM_LIBRARY_NAME)));
    if (lib.load()) {
        KCModule *(*create)(QWidget *, const char *);
        QByteArray factorymethod("create_");
        factorymethod += name.toLatin1();
        create = reinterpret_cast<KCModule *(*)(QWidget *, const char *)>(lib.resolve(factorymethod.constData()));
        if (create) {
            return create(nullptr, name.toLatin1().constData());
        } else {
            qCWarning(KLEOPATRA_LOG) << "Failed to load config module: " << name;
            return nullptr;
        }
    }
    qCWarning(KLEOPATRA_LOG) << "Failed to load library: " << KCM_LIBRARY_NAME;
    return nullptr;
}

void KleoPageConfigDialog::addModule(const QString &name)
{
    // We use a path relative to our installation location
    const QString path = qApp->applicationDirPath() +
                         QLatin1String("/../share/kservices5/") +
                         name + QLatin1String(".desktop");

    if (!QFile::exists(path)) {
        qCDebug(KLEOPATRA_LOG) << "Ignoring module for:" << name
            << "because the corresponding desktop file does not exist.";
        return;
    }

    KDesktopFile desktopModule(path);

    if (desktopModule.noDisplay()) {
        qCDebug(KLEOPATRA_LOG) << "Ignoring module for:" << name
            << "because it has no display set.";
        return;
    }

    KCModule *mod = loadModule(name);
    mModules << mod;

    const QString dName = desktopModule.readName();

    KPageWidgetItem *item = addPage(mod, dName);
    item->setIcon(QIcon::fromTheme(desktopModule.readIcon()));
    item->setHeader(desktopModule.readComment());

    connect(mod, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

    mHelpUrls.insert(dName, desktopModule.readDocPath());
}

void KleoPageConfigDialog::moduleChanged(bool state)
{
    KCModule *module = qobject_cast<KCModule*>(sender());
    qCDebug(KLEOPATRA_LOG) << "Module changed: " << state << " mod " << module;
    if (mChangedModules.contains(module)) {
        if (!state) {
            mChangedModules.removeAll(module);
        }
        return;
    }
    if (state) {
        mChangedModules << module;
    }
    clientChanged();
}

void KleoPageConfigDialog::clientChanged()
{
    const KPageWidgetItem *item = currentPage();
    if (!item) {
        return;
    }
    KCModule *module = qobject_cast<KCModule*>(item->widget());

    if (!module) {
        return;
    }
    qCDebug(KLEOPATRA_LOG) << "Client changed: " << " mod " << module;

    bool change = mChangedModules.contains(module);

    QPushButton *resetButton = buttonBox()->button(QDialogButtonBox::Reset);
    if (resetButton) {
        resetButton->setEnabled(change);
    }

    QPushButton *applyButton = buttonBox()->button(QDialogButtonBox::Apply);
    if (applyButton) {
        applyButton->setEnabled(change);
    }
}
