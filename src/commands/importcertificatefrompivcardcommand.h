/*  commands/importcertificatefrompivcardcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KLEOPATRA_COMMANDS_IMPORTCERTIFICATEFROMPIVCARDCOMMAND_H
#define KLEOPATRA_COMMANDS_IMPORTCERTIFICATEFROMPIVCARDCOMMAND_H

#include "importcertificatescommand.h"

namespace Kleo
{
namespace Commands
{

class ImportCertificateFromPIVCardCommand : public ImportCertificatesCommand
{
    Q_OBJECT
public:
    ImportCertificateFromPIVCardCommand(const std::string& cardSlot, const std::string &serialno);
    ~ImportCertificateFromPIVCardCommand() override;

private:
    void doStart() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
};

}
}

#endif /* KLEOPATRA_COMMANDS_IMPORTCERTIFICATEFROMPIVCARDCOMMAND_H */
