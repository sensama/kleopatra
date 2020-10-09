/*  commands/keytocardcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_KEYTOCARDCOMMAND_H__
#define __KLEOPATRA_COMMANDS_KEYTOCARDCOMMAND_H__

#include <commands/cardcommand.h>

#include <gpgme++/key.h>

namespace Kleo
{
namespace SmartCard
{
class Card;
}
}

namespace Kleo
{
namespace Commands
{

class KeyToCardCommand : public CardCommand
{
    Q_OBJECT
public:
    KeyToCardCommand(const GpgME::Subkey &subkey);
    KeyToCardCommand(const std::string &cardSlot, const std::string &serialNumber, const std::string &appName);
    ~KeyToCardCommand() override;

    static std::vector<std::shared_ptr<Kleo::SmartCard::Card> > getSuitableCards(const GpgME::Subkey &subkey);

public Q_SLOTS:
    void keyToOpenPGPCardDone(const GpgME::Error &err);
    void keyToPIVCardDone(const GpgME::Error &err);
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
