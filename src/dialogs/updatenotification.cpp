/*  dialogs/updatenotification.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2017 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

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
#include "updatenotification.h"

#include "utils/gnupg-helper.h"

#include "kleopatra_debug.h"

#include <QIcon>
#include <QGridLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QPushButton>
#include <QLabel>
#include <QUrl>
#include <QDateTime>

#include <KIconLoader>
#include <KLocalizedString>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KMessageBox>

#include <QGpgME/CryptoConfig>
#include <QGpgME/Protocol>

#include <gpgme++/gpgmefw.h>
#include <gpgme++/swdbresult.h>
#include <gpgme++/error.h>

using namespace Kleo;

namespace
{
static void gpgconf_set_update_check(bool value)
{
    auto conf = QGpgME::cryptoConfig();
    auto entry = conf->entry(QStringLiteral("dirmngr"),
                             QStringLiteral("Enforcement"),
                             QStringLiteral("allow-version-check"));
    if (!entry) {
        qCDebug(KLEOPATRA_LOG) << "allow-version-check entry not found";
    }
    entry->setBoolValue(value);
    conf->sync(true);
}
} // namespace

void UpdateNotification::checkUpdate(QWidget *parent, bool force)
{
#ifdef Q_OS_WIN
    if (force) {
        gpgconf_set_update_check(true);
    }

    const auto current = gpg4winVersion();
    GpgME::Error err;
    KConfigGroup updatecfg(KSharedConfig::openConfig(), "UpdateNotification");
    const auto lastshown = updatecfg.readEntry("LastShown", QDateTime());

    if (!force && lastshown.isValid() &&
        lastshown.addSecs(20 * 60 * 60) > QDateTime::currentDateTime()) {
        qDebug() << QDateTime::currentDateTime().addSecs(20 * 60 * 60);
        return;
    }

    const auto results = GpgME::SwdbResult::query("gpg4win",
                                                  current.toUtf8().constData(),
                                                  &err);
    if (err) {
        qCDebug(KLEOPATRA_LOG) << "update check failed: " << err.asString();
        return;
    }

    if (results.size() != 1) {
        /* Should not happen */
        qCDebug(KLEOPATRA_LOG) << "more then one result";
        return;
    }

    const auto result = results[0];

    if (result.update()) {
        const QString newVersion = QStringLiteral("%1.%2.%3").arg(result.version().major)
                                                             .arg(result.version().minor)
                                                             .arg(result.version().patch);
        qCDebug(KLEOPATRA_LOG) << "Have update to version:" << newVersion;
        UpdateNotification notifier(parent, newVersion);
        notifier.exec();
        updatecfg.writeEntry("LastShown", QDateTime::currentDateTime());
        updatecfg.sync();
    } else {
        qCDebug(KLEOPATRA_LOG) << "No update for:" << current;
        if (force) {
            KMessageBox::information(parent,
                                     i18nc("@info",
                                           "No update found in the available version database."),
                                     i18nc("@title", "Up to date"));
        }
    }
#else
    Q_UNUSED(parent);
    Q_UNUSED(force);
#endif
}


UpdateNotification::UpdateNotification(QWidget *parent, const QString &version) :
    QDialog(parent)
{
    resize(400, 200);
    auto lay = new QGridLayout(this);
    auto logo = new QLabel;
    logo->setMaximumWidth(110);

    setAttribute(Qt::WA_QuitOnClose, false);

    KIconLoader *const il = KIconLoader::global();
    const QString iconPath = il->iconPath(QLatin1String("gpg4win"),
                                          KIconLoader::User);
    logo->setPixmap(QIcon(iconPath).pixmap(100, 100));

    auto label = new QLabel;
    const QString boldVersion = QStringLiteral("<b>%1</b>").arg(version);
    label->setText (i18nc("%1 is the version number", "Version %1 is available.", boldVersion) +
        QStringLiteral("<br><br>") +
        i18nc("Link to NEWS style changelog",
              "See the <a href=\"https://www.gpg4win.org/change-history.html\">new features</a>."));
    label->setOpenExternalLinks(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setWordWrap(true);
    setWindowTitle(i18n("Update available!"));
    setWindowIcon(QIcon(QLatin1String("gpg4win")));

    lay->addWidget(logo, 0, 0);
    lay->addWidget(label, 0, 1);
    const auto chk = new QCheckBox (i18n("Show this notification for future updates."));
    lay->addWidget(chk, 1, 0, 1, -1);
    chk->setChecked(true);

    const auto bb = new QDialogButtonBox();
    const auto b = bb->addButton(i18n("&Get update"), QDialogButtonBox::AcceptRole);
    b->setDefault(true);
    b->setIcon(QIcon::fromTheme("arrow-down"));
    bb->addButton(QDialogButtonBox::Cancel);
    lay->addWidget(bb, 2, 0, 1, -1);
    connect (bb, &QDialogButtonBox::accepted, this, [this, chk]() {
            QDesktopServices::openUrl(QUrl("https://www.gpg4win.org/download.html"));
            KConfigGroup updatecfg(KSharedConfig::openConfig(), "UpdateNotification");

            updatecfg.writeEntry("NeverShow", !chk->isChecked());
            QDialog::accept();
        });
    connect (bb, &QDialogButtonBox::rejected, this, [this, chk]() {
            KConfigGroup updatecfg(KSharedConfig::openConfig(), "UpdateNotification");
            updatecfg.writeEntry("NeverShow", !chk->isChecked());
            QDialog::reject();
        });
}
