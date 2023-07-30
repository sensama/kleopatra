/* -*- mode: c++; c-basic-offset:4 -*-
    commands/changeroottrustcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <commands/command.h>

#include <gpgme++/key.h>

namespace Kleo
{
namespace Commands
{

class ChangeRootTrustCommand : public Command
{
    Q_OBJECT
public:
    explicit ChangeRootTrustCommand(KeyListController *parent);
    explicit ChangeRootTrustCommand(QAbstractItemView *view, KeyListController *parent);
    explicit ChangeRootTrustCommand(const GpgME::Key &key, KeyListController *parent);
    explicit ChangeRootTrustCommand(const GpgME::Key &key, QAbstractItemView *view, KeyListController *parent);
    ~ChangeRootTrustCommand() override;

    void setTrust(GpgME::Key::OwnerTrust trust);
    GpgME::Key::OwnerTrust trust() const;

    void setTrustListFile(const QString &file);
    QString trustListFile() const;

    /* reimp */ static Restrictions restrictions()
    {
        return OnlyOneKey | MustBeCMS | MustBeRoot;
    }

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
};

class TrustRootCommand : public ChangeRootTrustCommand
{
public:
    explicit TrustRootCommand(KeyListController *parent)
        : ChangeRootTrustCommand(parent)
    {
        setTrust(GpgME::Key::Ultimate);
    }
    explicit TrustRootCommand(QAbstractItemView *view, KeyListController *parent)
        : ChangeRootTrustCommand(view, parent)
    {
        setTrust(GpgME::Key::Ultimate);
    }
    explicit TrustRootCommand(const GpgME::Key &key, KeyListController *parent)
        : ChangeRootTrustCommand(key, parent)
    {
        setTrust(GpgME::Key::Ultimate);
    }
    explicit TrustRootCommand(const GpgME::Key &key, QAbstractItemView *view, KeyListController *parent)
        : ChangeRootTrustCommand(key, view, parent)
    {
        setTrust(GpgME::Key::Ultimate);
    }

    /* reimp */ static Restrictions restrictions()
    {
        return ChangeRootTrustCommand::restrictions() | MustBeUntrustedRoot;
    }
};

class DistrustRootCommand : public ChangeRootTrustCommand
{
public:
    explicit DistrustRootCommand(KeyListController *parent)
        : ChangeRootTrustCommand(parent)
    {
        setTrust(GpgME::Key::Never);
    }
    explicit DistrustRootCommand(QAbstractItemView *view, KeyListController *parent)
        : ChangeRootTrustCommand(view, parent)
    {
        setTrust(GpgME::Key::Never);
    }
    explicit DistrustRootCommand(const GpgME::Key &key, KeyListController *parent)
        : ChangeRootTrustCommand(key, parent)
    {
        setTrust(GpgME::Key::Never);
    }
    explicit DistrustRootCommand(const GpgME::Key &key, QAbstractItemView *view, KeyListController *parent)
        : ChangeRootTrustCommand(key, view, parent)
    {
        setTrust(GpgME::Key::Never);
    }

    /* reimp */ static Restrictions restrictions()
    {
        return ChangeRootTrustCommand::restrictions() | MustBeTrustedRoot;
    }
};

}
}
