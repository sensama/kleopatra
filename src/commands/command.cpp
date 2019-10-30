/* -*- mode: c++; c-basic-offset:4 -*-
    commands/command.cpp

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

#include "command.h"
#include "command_p.h"

#include "signencryptfilescommand.h"
#include "importcertificatefromfilecommand.h"
#include "decryptverifyfilescommand.h"
#include "detailscommand.h"
#include "lookupcertificatescommand.h"
#include "checksumverifyfilescommand.h"

#include <Libkleo/KeyCache>
#include <Libkleo/Classify>


#include <view/tabwidget.h>

#include "kleopatra_debug.h"
#include <KWindowSystem>

#include <QAbstractItemView>
#include <QFileInfo>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

Command::Private::Private(Command *qq, KeyListController *controller)
    : q(qq),
      autoDelete(true),
      warnWhenRunningAtShutdown(true),
      indexes_(),
      view_(),
      parentWId(0),
      controller_(controller)
{

}

Command::Private::~Private()
{
    qCDebug(KLEOPATRA_LOG);
}

Command::Command(KeyListController *p)
    : QObject(p), d(new Private(this, p))
{
    if (p) {
        p->registerCommand(this);
    }
}

Command::Command(QAbstractItemView *v, KeyListController *p)
    : QObject(p), d(new Private(this, p))
{
    if (p) {
        p->registerCommand(this);
    }
    if (v) {
        setView(v);
    }
}

Command::Command(Private *pp)
    : QObject(pp->controller_), d(pp)
{
    if (pp->controller_) {
        pp->controller_->registerCommand(this);
    }
}

Command::Command(QAbstractItemView *v, Private *pp)
    : QObject(pp->controller_), d(pp)
{
    if (pp->controller_) {
        pp->controller_->registerCommand(this);
    }
    if (v) {
        setView(v);
    }
}

Command::Command(const GpgME::Key &key)
    : QObject(nullptr), d(new Private(this, nullptr))
{
    d->keys_ = std::vector<Key>(1, key);
}

Command::Command(const std::vector<GpgME::Key> &keys)
    : QObject(nullptr), d(new Private(this, nullptr))
{
    d->keys_ = keys;
}

Command::Command(const Key &key, Private *pp)
    : QObject(nullptr), d(pp)
{
    d->keys_ = std::vector<Key>(1, key);
}

Command::Command(const std::vector<GpgME::Key> &keys, Private *pp)
    : QObject(nullptr), d(pp)
{
    d->keys_ = keys;
}

Command::~Command()
{
    qCDebug(KLEOPATRA_LOG);
}

void Command::setAutoDelete(bool on)
{
    d->autoDelete = on;
}

bool Command::autoDelete() const
{
    return d->autoDelete;
}

void Command::setWarnWhenRunningAtShutdown(bool on)
{
    d->warnWhenRunningAtShutdown = on;
}

bool Command::warnWhenRunningAtShutdown() const
{
    return d->warnWhenRunningAtShutdown;
}

void Command::setParentWidget(QWidget *widget)
{
    d->parentWidget_ = widget;
}

void Command::setParentWId(WId wid)
{
    d->parentWId = wid;
}

void Command::setView(QAbstractItemView *view)
{
    if (view == d->view_) {
        return;
    }
    d->view_ = view;
    if (!view || !d->indexes_.empty()) {
        return;
    }
    const QItemSelectionModel *const sm = view->selectionModel();
    if (!sm) {
        qCWarning(KLEOPATRA_LOG) << "view " << (void *)view << " has no selectionModel!";
        return;
    }
    const QList<QModelIndex> selected = sm->selectedRows();
    if (!selected.empty()) {
        std::copy(selected.begin(), selected.end(), std::back_inserter(d->indexes_));
        return;
    }
}

void Command::setIndex(const QModelIndex &idx)
{
    d->indexes_.clear();
    d->indexes_.push_back(idx);
}

void Command::setIndexes(const QList<QModelIndex> &idx)
{
    d->indexes_.clear();
    std::copy(idx.begin(), idx.end(), std::back_inserter(d->indexes_));
}

void Command::setKey(const Key &key)
{
    d->keys_.clear();
    if (!key.isNull()) {
        d->keys_.push_back(key);
    }
}

void Command::setKeys(const std::vector<Key> &keys)
{
    d->keys_ = keys;
}

void Command::start()
{
    doStart();
}

void Command::cancel()
{
    qCDebug(KLEOPATRA_LOG) << metaObject()->className();
    doCancel();
    Q_EMIT canceled();
}

void Command::addTemporaryView(const QString &title, AbstractKeyListSortFilterProxyModel *proxy, const QString &tabToolTip)
{
    if (TabWidget *const tw = d->controller_ ? d->controller_->tabWidget() : nullptr)
        if (QAbstractItemView *const v = tw->addTemporaryView(title, proxy, tabToolTip)) {
            setView(v);
        }
}

void Command::applyWindowID(QWidget *w) const
{
    if (w) {
        if (d->parentWId) {
            if (QWidget *pw = QWidget::find(d->parentWId)) {
                w->setParent(pw, w->windowFlags());
            } else {
                w->setAttribute(Qt::WA_NativeWindow, true);
                KWindowSystem::setMainWindow(w->windowHandle(), d->parentWId);
            }
        } else {
            w->setParent(d->parentWidgetOrView(), w->windowFlags());
        }
    }
}

// static
QVector <Command *> Command::commandsForFiles(const QStringList &files)
{
    QStringList importFiles, decryptFiles, encryptFiles, checksumFiles;
    QVector <Command *> cmds;
    for (const QString &fileName : files) {
        const unsigned int classification = classify(fileName);

        if (classification & Class::AnyCertStoreType) {
            importFiles << fileName;
        } else if (classification & Class::AnyMessageType) {
            // For any message we decrypt / verify. This includes
            // the class CipherText
            decryptFiles << fileName;
        } else if (isChecksumFile(fileName)) {
            checksumFiles << fileName;
        } else {
            QFileInfo fi(fileName);
            if (fi.isReadable()) {
                encryptFiles << fileName;
            }
        }
    }
    if (!importFiles.isEmpty()) {
        cmds << new ImportCertificateFromFileCommand(importFiles, nullptr);
    }
    if (!decryptFiles.isEmpty()) {
        cmds << new DecryptVerifyFilesCommand(decryptFiles, nullptr);
    }
    if (!encryptFiles.isEmpty()) {
        cmds << new SignEncryptFilesCommand(encryptFiles, nullptr);
    }
    if (!checksumFiles.isEmpty()) {
        cmds << new ChecksumVerifyFilesCommand(checksumFiles, nullptr);
    }
    return cmds;
}

// static
Command *Command::commandForQuery(const QString &query)
{
    const auto cache = Kleo::KeyCache::instance();
    GpgME::Key key = cache->findByKeyIDOrFingerprint(query.toLocal8Bit().data());

    if (key.isNull() && query.size() > 16) {
        // Try to find by subkeyid
        std::vector<std::string> id;
        id.push_back(query.right(16).toStdString());
        auto keys = cache->findSubkeysByKeyID(id);
        if (keys.size()) {
            key = keys[0].parent();
        }
    }
    if (key.isNull()) {
        return new LookupCertificatesCommand(query, nullptr);
    } else {
        return new DetailsCommand(key, nullptr);
    }
}
