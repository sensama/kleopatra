/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/addUserIDdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

#include <utils/pimpl_ptr.h>

class QString;

namespace Kleo
{
namespace Dialogs
{

class AddUserIDDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QString name    READ name    WRITE setName)
    Q_PROPERTY(QString email   READ email   WRITE setEmail)
    Q_PROPERTY(QString comment READ comment WRITE setComment)
public:
    explicit AddUserIDDialog(QWidget *parent = nullptr);
    ~AddUserIDDialog() override;

    void setName(const QString &name);
    QString name() const;

    void setEmail(const QString &email);
    QString email() const;

    void setComment(const QString &comment);
    QString comment() const;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotUserIDChanged())
};

}
}

