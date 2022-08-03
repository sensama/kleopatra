/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/selftestdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

#include <utils/pimpl_ptr.h>

#include <memory>
#include <vector>

namespace Kleo
{

class SelfTest;

namespace Dialogs
{

class SelfTestDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(bool runAtStartUp READ runAtStartUp WRITE setRunAtStartUp)
public:
    explicit SelfTestDialog(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    explicit SelfTestDialog(const std::vector<std::shared_ptr<SelfTest>> &tests, QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~SelfTestDialog() override;

    void setAutomaticMode(bool automatic);

    void addSelfTest(const std::shared_ptr<SelfTest> &test);
    void addSelfTests(const std::vector<std::shared_ptr<SelfTest>> &tests);

    void setRunAtStartUp(bool run);
    bool runAtStartUp() const;

public Q_SLOTS:
    void clear();

Q_SIGNALS:
    void updateRequested();

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

}
}

