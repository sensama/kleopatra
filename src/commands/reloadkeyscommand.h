/* -*- mode: c++; c-basic-offset:4 -*-
    reloadkeyscommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_RELOADKEYSCOMMAND_H__
#define __KLEOPATRA_RELOADKEYSCOMMAND_H__

#include <commands/command.h>

namespace  Kleo
{

class ReloadKeysCommand : public Command
{
    Q_OBJECT
public:
    explicit ReloadKeysCommand(KeyListController *parent);
    ReloadKeysCommand(QAbstractItemView *view, KeyListController *parent);
    ~ReloadKeysCommand() override;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;

    Q_PRIVATE_SLOT(d_func(), void keyListingDone(GpgME::KeyListResult))
};
}

#endif // __KLEOPATRA_RELOADKEYSCOMMAND_H__
