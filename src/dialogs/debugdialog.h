// SPDX-FileCopyrightText: 2024 g10 Code GmbH
// SPDX-FileContributor: Tobias Fella <tobias.fella@gnupg.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

namespace Kleo
{

class DebugDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DebugDialog(QWidget *parent);
    ~DebugDialog() override;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

}
