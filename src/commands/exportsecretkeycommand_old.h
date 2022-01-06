/* -*- mode: c++; c-basic-offset:4 -*-
    commands/exportsecretkeycommand_old.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <commands/gnupgprocesscommand.h>

#include <QString>

namespace Kleo
{
namespace Commands
{
namespace Compat
{

class ExportSecretKeyCommand : public GnuPGProcessCommand
{
    Q_OBJECT
public:
    explicit ExportSecretKeyCommand(QAbstractItemView *view, KeyListController *parent);
    explicit ExportSecretKeyCommand(KeyListController *parent);
    explicit ExportSecretKeyCommand(const GpgME::Key &key);
    ~ExportSecretKeyCommand() override;

    void setFileName(const QString &fileName);
    QString fileName() const
    {
        return m_filename;
    }

    /* reimp */ static Restrictions restrictions()
    {
        return OnlyOneKey | NeedSecretKey;
    }

private:
    bool preStartHook(QWidget *) const override;
    void postSuccessHook(QWidget *) override;

    QStringList arguments() const override;

    QString errorCaption() const override;
    QString successCaption() const override;

    QString crashExitMessage(const QStringList &) const override;
    QString errorExitMessage(const QStringList &) const override;
    QString successMessage(const QStringList &) const override;

private:
    mutable QString m_filename;
    mutable bool m_armor;
    bool mHasError = false;
};

}
}
}

