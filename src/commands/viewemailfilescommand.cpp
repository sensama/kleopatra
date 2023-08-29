// This file is part of Kleopatra, the KDE keymanager
// SPDX-FileCopyrightText: 2023 g10 Code GmbH
// SPDX-FileContributor: Carl Schwan <carl.schwan@gnupg.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "viewemailfilescommand.h"

#include "command_p.h"
#include "dialogs/messageviewerdialog.h"

using namespace Kleo::Commands;

class ViewEmailFilesCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::ViewEmailFilesCommand;
    ViewEmailFilesCommand *q_func() const
    {
        return static_cast<ViewEmailFilesCommand *>(q);
    }

public:
    Private(ViewEmailFilesCommand *qq, KeyListController *c);
    ~Private() override;

    QPointer<MessageViewerDialog> dialog;
    QStringList files;

    void ensureDialogCreated();
};

ViewEmailFilesCommand::Private::Private(ViewEmailFilesCommand *qq, KeyListController *c)
    : Command::Private(qq, c)
{
}

ViewEmailFilesCommand::Private::~Private() = default;

void ViewEmailFilesCommand::Private::ensureDialogCreated()
{
    auto dlg = new MessageViewerDialog(files[0]);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    applyWindowID(dlg);
    connect(dlg, &MessageViewerDialog::finished, q_func(), [this] {
        finished();
    });
    dlg->show();

    dialog = dlg;
}

ViewEmailFilesCommand::Private *ViewEmailFilesCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const ViewEmailFilesCommand::Private *ViewEmailFilesCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

ViewEmailFilesCommand::ViewEmailFilesCommand(const QStringList &files, KeyListController *c)
    : Command(new Private(this, c))
{
    Q_ASSERT(!files.isEmpty());

    setWarnWhenRunningAtShutdown(false);

    d->files = files;
}

ViewEmailFilesCommand::~ViewEmailFilesCommand() = default;

void ViewEmailFilesCommand::doStart()
{
    d->ensureDialogCreated();
}

void ViewEmailFilesCommand::doCancel()
{
    if (d->dialog) {
        d->dialog->close();
    }
}
