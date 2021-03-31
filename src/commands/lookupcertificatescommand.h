/* -*- mode: c++; c-basic-offset:4 -*-
    commands/lookupcertificatescommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <commands/importcertificatescommand.h>

namespace Kleo
{
namespace Commands
{

class LookupCertificatesCommand : public ImportCertificatesCommand
{
    Q_OBJECT
public:
    explicit LookupCertificatesCommand(QAbstractItemView *view, KeyListController *parent);
    explicit LookupCertificatesCommand(KeyListController *parent);
    explicit LookupCertificatesCommand(const QString &fingerPrint, KeyListController *parent);
    ~LookupCertificatesCommand() override;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotSearchTextChanged(QString))
    Q_PRIVATE_SLOT(d_func(), void slotNextKey(GpgME::Key))
    Q_PRIVATE_SLOT(d_func(), void slotKeyListResult(GpgME::KeyListResult))
    Q_PRIVATE_SLOT(d_func(), void slotImportRequested(std::vector<GpgME::Key>))
    Q_PRIVATE_SLOT(d_func(), void slotDetailsRequested(GpgME::Key))
    Q_PRIVATE_SLOT(d_func(), void slotSaveAsRequested(std::vector<GpgME::Key>))
    Q_PRIVATE_SLOT(d_func(), void slotDialogRejected())
};

}
}

