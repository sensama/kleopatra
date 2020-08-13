/* -*- mode: c++; c-basic-offset:4 -*-
    commands/decryptverifyclipboardcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMMANDS_DECRYPTVERIFYCLIPBOARDCOMMAND_H__
#define __KLEOPATRA_COMMMANDS_DECRYPTVERIFYCLIPBOARDCOMMAND_H__

#include <commands/command.h>

#ifndef QT_NO_CLIPBOARD

#include <utils/types.h>

namespace Kleo
{
namespace Commands
{

class DecryptVerifyClipboardCommand : public Command
{
    Q_OBJECT
public:
    explicit DecryptVerifyClipboardCommand(QAbstractItemView *view, KeyListController *parent);
    explicit DecryptVerifyClipboardCommand(KeyListController *parent);
    ~DecryptVerifyClipboardCommand() override;

    static bool canDecryptVerifyCurrentClipboard();

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotControllerDone())
    Q_PRIVATE_SLOT(d_func(), void slotControllerError(int, QString))
};

}
}

#endif // QT_NO_CLIPBOARD

#endif // __KLEOPATRA_COMMMANDS_DECRYPTVERIFYCLIPBOARDCOMMAND_H__
