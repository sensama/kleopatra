/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/expirydialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

#include <utils/pimpl_ptr.h>

class QDate;

namespace Kleo
{
namespace Dialogs
{

class ExpiryDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QDate dateOfExpiry READ dateOfExpiry WRITE setDateOfExpiry)
public:
    explicit ExpiryDialog(QWidget *parent = nullptr);
    ~ExpiryDialog();

    void setDateOfExpiry(const QDate &date);
    QDate dateOfExpiry() const;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotInAmountChanged())
    Q_PRIVATE_SLOT(d, void slotInUnitChanged())
    Q_PRIVATE_SLOT(d, void slotOnDateChanged())
};

}
}

