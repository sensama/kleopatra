/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/addemaildialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019 g 10 Code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

#include <memory>

class QString;

namespace Kleo
{
namespace Dialogs
{

class AddEmailDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddEmailDialog(QWidget *parent = nullptr);
    ~AddEmailDialog();

    void setEmail(const QString &email);
    QString email() const;

    bool advancedSelected();
private:
    class Private;
    std::shared_ptr<Private> d;
};

}
}
