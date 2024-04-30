// SPDX-FileCopyrightText: 2024 g10 Code GmbH
// SPDX-FileContributor: Tobias Fella <tobias.fella@gnupg.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <commands/cardcommand.h>
#include <commands/command.h>

#include <gpgme++/key.h>

#include <memory>

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

class CertificateToCardCommand : public CardCommand
{
    Q_OBJECT
public:
    CertificateToCardCommand(QAbstractItemView *view, KeyListController *controller);
    ~CertificateToCardCommand() override;

    /* reimp */ static Restrictions restrictions()
    {
        return NeedSelection | OnlyOneKey | NeedSecretKey | SuitableForCard | MustBeOpenPGP | MustBeValid;
    }

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
