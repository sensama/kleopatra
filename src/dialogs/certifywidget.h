#ifndef SRC_VIEW_CERTIFYWIDGET_H
#define SRC_VIEW_CERTIFYWIDGET_H
/*  dialogs/certifywidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019 g 10code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QWidget>

#include <vector>
#include <memory>

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

    /* Set the key to certify */
    void setTarget(const GpgME::Key &key);

    /* Get the key to certify */
    GpgME::Key target() const;

    /* Select specific user ids. Default: all */
    void selectUserIDs(const std::vector<GpgME::UserID> &uids);

    /* The user ids that should be signed */
    std::vector<unsigned int> selectedUserIDs() const;

    /* The secret key selected */
    GpgME::Key secKey() const;

    /* Should the signature be exportable */
    bool exportableSelected() const;

    /* Additional tags for the key */
    QString tags() const;

    /* Should the signed key be be published */
    bool publishSelected() const;

private:
    class Private;
    std::shared_ptr<Private> d;
};

} // namespace Kleo

#endif // SRC_VIEW_CERTIFYWIDGET_H
