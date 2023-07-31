/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/revokekeydialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <config-kleopatra.h>

#include <QDialog>

#include <memory>

namespace GpgME
{
class Key;
#if QGPGME_SUPPORTS_KEY_REVOCATION
enum class RevocationReason;
#endif
}

namespace Kleo
{

class RevokeKeyDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RevokeKeyDialog(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~RevokeKeyDialog() override;

    void setKey(const GpgME::Key &key);

#if QGPGME_SUPPORTS_KEY_REVOCATION
    GpgME::RevocationReason reason() const;
#endif
    QString description() const;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

} // namespace Kleo
