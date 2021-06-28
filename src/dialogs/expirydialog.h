/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/expirydialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

#include <memory.h>

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
    ~ExpiryDialog() override;

    void setDateOfExpiry(const QDate &date);
    QDate dateOfExpiry() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}

