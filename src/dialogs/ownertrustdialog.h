/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/ownertrustdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

#include <utils/pimpl_ptr.h>

#include <gpgme++/key.h>

namespace Kleo
{
namespace Dialogs
{

class OwnerTrustDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OwnerTrustDialog(QWidget *parent = nullptr);
    ~OwnerTrustDialog() override;

    void setFormattedCertificateName(const QString &formatted);
    QString formattedCertificateName() const;

    void setHasSecretKey(bool secret);
    bool hasSecretKey() const;

    void setAdvancedMode(bool advanced);
    bool isAdvancedMode() const;

    void setOwnerTrust(GpgME::Key::OwnerTrust);
    GpgME::Key::OwnerTrust ownerTrust() const;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotTrustLevelChanged())
};

}
}

