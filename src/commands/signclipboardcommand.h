/* -*- mode: c++; c-basic-offset:4 -*-
    commands/signclipboardcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <commands/command.h>

#ifndef QT_NO_CLIPBOARD

#include <utils/types.h>

#include <gpgme++/global.h>

namespace Kleo
{
namespace Commands
{

class SignClipboardCommand : public Command
{
    Q_OBJECT
public:
    explicit SignClipboardCommand(GpgME::Protocol protocol, QAbstractItemView *view, KeyListController *parent);
    explicit SignClipboardCommand(GpgME::Protocol protocol, KeyListController *parent);
    ~SignClipboardCommand() override;

    static bool canSignCurrentClipboard();

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotSignersResolved())
    Q_PRIVATE_SLOT(d_func(), void slotControllerDone())
    Q_PRIVATE_SLOT(d_func(), void slotControllerError(int, QString))
};

}
}

#endif // QT_NO_CLIPBOARD

