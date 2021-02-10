/* -*- mode: c++; c-basic-offset:4 -*-
    commands/editgroupcommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "editgroupcommand.h"

#include "command_p.h"

#include "dialogs/editgroupdialog.h"

#include <Libkleo/KeyCache>
#include <Libkleo/KeyGroup>

#include <KLocalizedString>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;

class EditGroupCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::EditGroupCommand;
    EditGroupCommand *q_func() const
    {
        return static_cast<EditGroupCommand *>(q);
    }
public:
    explicit Private(EditGroupCommand *qq, const KeyGroup &group, QWidget *parent);
    ~Private();

private:
    void start();

    void slotDialogAccepted();
    void slotDialogRejected();

    void ensureDialogCreated();

private:
    KeyGroup group;
    QPointer<EditGroupDialog> dialog;
};

EditGroupCommand::Private *EditGroupCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const EditGroupCommand::Private *EditGroupCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

EditGroupCommand::Private::Private(EditGroupCommand *qq, const KeyGroup &group, QWidget *parent)
    : Command::Private(qq, parent)
    , group(group)
{
}

EditGroupCommand::Private::~Private()
{
}

void EditGroupCommand::Private::start()
{
    ensureDialogCreated();

    dialog->setWindowTitle(i18nc("@title:window", "Edit Group"));
    dialog->setGroupName(group.name());
    const KeyGroup::Keys &keys = group.keys();
    dialog->setGroupKeys(std::vector<GpgME::Key>(keys.cbegin(), keys.cend()));
    dialog->show();
}

void EditGroupCommand::Private::slotDialogAccepted()
{
    group.setName(dialog->groupName());
    group.setKeys(dialog->groupKeys());
    KeyCache::mutableInstance()->update(group);

    finished();
}

void EditGroupCommand::Private::slotDialogRejected()
{
    canceled();
}

void EditGroupCommand::Private::ensureDialogCreated()
{
    if (dialog) {
        return;
    }

    dialog = new EditGroupDialog;
    applyWindowID(dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &QDialog::accepted, q, [this] () { slotDialogAccepted(); });
    connect(dialog, &QDialog::rejected, q, [this] () { slotDialogRejected(); });
}

EditGroupCommand::EditGroupCommand(const KeyGroup &group, QWidget *parent)
    : Command(new Private(this, group, parent))
{
}

EditGroupCommand::~EditGroupCommand()
{
}

void EditGroupCommand::doStart()
{
    d->start();
}

void EditGroupCommand::doCancel()
{
}

#undef d
#undef q
