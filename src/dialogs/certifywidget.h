#pragma once
/*  dialogs/certifywidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019 g 10code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QWidget>

#include <memory>
#include <vector>

namespace GpgME
{
class Key;
class UserID;
} // namespace GpgME

namespace Kleo
{
/** Widget for OpenPGP certification. */
class CertifyWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CertifyWidget(QWidget *parent = nullptr);

    ~CertifyWidget() override;

    /* Set the key to certify */
    void setTarget(const GpgME::Key &key);

    /* Get the key to certify */
    GpgME::Key target() const;

    /* Select specific user IDs. Default: all */
    void selectUserIDs(const std::vector<GpgME::UserID> &uids);

    /* The user IDs that should be signed */
    std::vector<GpgME::UserID> selectedUserIDs() const;

    /* The secret key selected */
    GpgME::Key secKey() const;

    /* Should the signature be exportable */
    bool exportableSelected() const;

    /* Additional tags for the key */
    QString tags() const;

    /* Should the signed key be be published */
    bool publishSelected() const;

    /* Whether a trust signature should be created */
    bool trustSignatureSelected() const;

    /* The domain to use to limit the scope of the trust signature */
    QString trustSignatureDomain() const;

    /* The expiration date to use for the certification */
    QDate expirationDate() const;

    bool isValid() const;

Q_SIGNALS:
    void changed() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace Kleo
