/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/revokecertificationwidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <vector>

namespace GpgME
{
class Key;
class UserID;
}

namespace Kleo
{
/** Widget for revoking OpenPGP certifications. */
class RevokeCertificationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RevokeCertificationWidget(QWidget *parent = nullptr);
    ~RevokeCertificationWidget() override;

    /* Set the key to revoke certifications of */
    void setTarget(const GpgME::Key &key);

    /* Get the key to revoke certifications of */
    GpgME::Key target() const;

    /* Select specific user ids. Default: all */
    void setSelectUserIDs(const std::vector<GpgME::UserID> &uids);

    /* The user ids whose certifications shall be revoked */
    std::vector<GpgME::UserID> selectedUserIDs() const;

    /* Set the selected certification key. Default: last used key */
    void setCertificationKey(const GpgME::Key &key);

    /* The selected certification key */
    GpgME::Key certificationKey() const;

    /* Whether the revocations shall be published */
    bool publishSelected() const;

    void saveConfig() const;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

} // namespace Kleo

