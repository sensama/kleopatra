/*  commands/keytocardcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_KEYTOCARDCOMMAND_H__
#define __KLEOPATRA_COMMANDS_KEYTOCARDCOMMAND_H__

#include <commands/command.h>

#include <string>
#include <gpgme++/key.h>

namespace Kleo
{
namespace Commands
{

class KeyToCardCommand : public Command
{
    Q_OBJECT
public:
    KeyToCardCommand(const GpgME::Subkey &key, const std::string &serialno);
    ~KeyToCardCommand() override;

    static bool supported();


public Q_SLOTS:
    void keyToCardDone(const GpgME::Error &err);
    void deleteDone(const GpgME::Error &err);

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
};

}
}

#endif /* __KLEOPATRA_COMMANDS_KEYTOCARDCOMMAND_H__ */
