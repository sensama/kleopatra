/* -*- mode: c++; c-basic-offset:4 -*-
    commands/detailscommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_DETAILSCOMMAND_H__
#define __KLEOPATRA_COMMANDS_DETAILSCOMMAND_H__

#include <commands/command.h>

namespace GpgME
{
class Key;
}

namespace Kleo
{
namespace Commands
{

class DetailsCommand : public Command
{
    Q_OBJECT
public:
    explicit DetailsCommand(KeyListController *parent);
    explicit DetailsCommand(QAbstractItemView *view, KeyListController *parent);
    explicit DetailsCommand(const GpgME::Key &key, KeyListController *parent);
    explicit DetailsCommand(const GpgME::Key &key, QAbstractItemView *view, KeyListController *parent);
    ~DetailsCommand() override;

    /* reimp */ static Restrictions restrictions()
    {
        return OnlyOneKey;
    }

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotDialogClosed())
};

}
}

#endif /* __KLEOPATRA_COMMANDS_DETAILSCOMMAND_H__ */
