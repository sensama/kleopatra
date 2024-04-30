// SPDX-FileCopyrightText: 2024 g10 Code GmbH
// SPDX-FileContributor: Tobias Fella <tobias.fella@gnupg.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include <gpgme++/key.h>

namespace Kleo
{
namespace Dialogs
{

class CopyToSmartcardDialog : public QDialog
{
    Q_OBJECT
public:
    enum BackupChoice {
        FileBackup,
        PrintBackup,
        ExistingBackup,
        KeepKey,
    };

    explicit CopyToSmartcardDialog(QWidget *parent = nullptr);
    ~CopyToSmartcardDialog() override;

    GpgME::Key key() const;
    void setKey(const GpgME::Key &key);

    QString cardDisplayName() const;
    void setCardDisplayName(const QString &cardDisplayName);

    BackupChoice backupChoice() const;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

}
}
