// This file is part of Kleopatra, the KDE keymanager
// SPDX-FileCopyrightText: 2023 g10 Code GmbH
// SPDX-FileContributor: Carl Schwan <carl.schwan@gnupg.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include <memory>

class MessageViewerDialog : public QDialog
{
    Q_OBJECT

public:
    MessageViewerDialog(const QString &fileName, QWidget *parent = nullptr);
    ~MessageViewerDialog() override;

private:
    class Private;
    std::unique_ptr<Private> const d;
};
