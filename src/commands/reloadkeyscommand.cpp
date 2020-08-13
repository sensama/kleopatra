/* -*- mode: c++; c-basic-offset:4 -*-
    reloadkeyscommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "reloadkeyscommand.h"
#include "smartcard/readerstatus.h"
#include "command_p.h"

#include <Libkleo/KeyCache>

#include "kleopatra_debug.h"

#include <gpgme++/keylistresult.h>

using namespace Kleo;
using namespace GpgME;

class ReloadKeysCommand::Private : public Command::Private
{
    friend class ::Kleo::ReloadKeysCommand;
public:
    Private(ReloadKeysCommand *qq, KeyListController *controller);
    ~Private();

    void keyListingDone(const KeyListResult &result);
};

ReloadKeysCommand::Private *ReloadKeysCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const ReloadKeysCommand::Private *ReloadKeysCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

ReloadKeysCommand::ReloadKeysCommand(KeyListController *p)
    : Command(new Private(this, p))
{

}

ReloadKeysCommand::ReloadKeysCommand(QAbstractItemView *v, KeyListController *p)
    : Command(v, new Private(this, p))
{

}

ReloadKeysCommand::~ReloadKeysCommand() {}

ReloadKeysCommand::Private::Private(ReloadKeysCommand *qq, KeyListController *controller)
    : Command::Private(qq, controller)
{
}

ReloadKeysCommand::Private::~Private() {}

void ReloadKeysCommand::Private::keyListingDone(const KeyListResult &result)
{
    if (result.error()) { // ### Show error message here?
        qCritical() << "Error occurred during key listing: " << result.error().asString();
    }
    finished();
}

#define d d_func()

void ReloadKeysCommand::doStart()
{
    const auto view = d->parentWidgetOrView();
    if (view && !view->isVisible()) {
        // Small hack to make redisplay also work nicely when the keylist
        // is not currently the active widget.
        SmartCard::ReaderStatus::mutableInstance()->updateStatus();
        d->finished();
        return;
    }
    connect(KeyCache::mutableInstance().get(), SIGNAL(keyListingDone(GpgME::KeyListResult)),
            this, SLOT(keyListingDone(GpgME::KeyListResult)));
    KeyCache::mutableInstance()->startKeyListing();
}

void ReloadKeysCommand::doCancel()
{
    KeyCache::mutableInstance()->cancelKeyListing();
}

#undef d

#include "moc_reloadkeyscommand.cpp"
