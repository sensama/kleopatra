/* -*- mode: c++; c-basic-offset:4 -*-
    commands/encryptclipboardcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMMANDS_ENCRYPTCLIPBOARDCOMMAND_H__
#define __KLEOPATRA_COMMMANDS_ENCRYPTCLIPBOARDCOMMAND_H__

#include <commands/command.h>

#ifndef QT_NO_CLIPBOARD

#include <utils/types.h>

namespace Kleo
{
namespace Commands
{

class EncryptClipboardCommand : public Command
{
    Q_OBJECT
public:
    explicit EncryptClipboardCommand(QAbstractItemView *view, KeyListController *parent);
    explicit EncryptClipboardCommand(KeyListController *parent);
    ~EncryptClipboardCommand() override;

    static bool canEncryptCurrentClipboard();

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotRecipientsResolved())
    Q_PRIVATE_SLOT(d_func(), void slotControllerDone())
    Q_PRIVATE_SLOT(d_func(), void slotControllerError(int, QString))
};

}
}

#endif // QT_NO_CLIPBOARD

#endif // __KLEOPATRA_COMMMANDS_ENCRYPTCLIPBOARDCOMMAND_H__
