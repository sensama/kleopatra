/* -*- mode: c++; c-basic-offset:4 -*-
    commands/learncardkeyscommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <commands/gnupgprocesscommand.h>

#include <gpgme++/global.h>

namespace Kleo
{
namespace Commands
{

class LearnCardKeysCommand : public GnuPGProcessCommand
{
    Q_OBJECT
public:
    explicit LearnCardKeysCommand(GpgME::Protocol proto);
    ~LearnCardKeysCommand() override;

    GpgME::Protocol protocol() const;

    /* reimp */ static Restrictions restrictions()
    {
        return AnyCardCanLearnKeys;
    }

private:
    void doStart() override;

    QStringList arguments() const override;

    QString errorCaption() const override;
    QString successCaption() const override;

    QString crashExitMessage(const QStringList &) const override;
    QString errorExitMessage(const QStringList &) const override;
    QString successMessage(const QStringList &) const override;

private:
    GpgME::Protocol m_protocol;
};

}
}

