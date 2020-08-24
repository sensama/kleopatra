/*  commands/pivgeneratecardkeycommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef __KLEOPATRA_COMMMANDS_PIVGENERATECARDKEYCOMMAND_H__
#define __KLEOPATRA_COMMMANDS_PIVGENERATECARDKEYCOMMAND_H__

#include "cardcommand.h"

namespace GpgME
{
class Error;
}

namespace Kleo
{
namespace Commands
{

class PIVGenerateCardKeyCommand : public CardCommand
{
    Q_OBJECT
public:
    explicit PIVGenerateCardKeyCommand(const std::string &serialNumber, QWidget *parent);
    ~PIVGenerateCardKeyCommand() override;

    void setKeyRef(const std::string &keyref);

private:
    void doStart() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotGenerateKeyResult(GpgME::Error))
    Q_PRIVATE_SLOT(d_func(), void slotAuthenticateResult(GpgME::Error))
};

} // namespace Commands
} // namespace Kleo

#endif // __KLEOPATRA_COMMMANDS_PIVGENERATECARDKEYCOMMAND_H__
