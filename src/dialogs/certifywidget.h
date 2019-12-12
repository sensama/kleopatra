#ifndef SRC_VIEW_CERTIFYWIDGET_H
#define SRC_VIEW_CERTIFYWIDGET_H
/*  dialogs/certifywidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2019 by g10code GmbH

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
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

    /* Additional remarks (search tags) for the key */
    QString remarks() const;

    /* Should the signed key be be published */
    bool publishSelected() const;

private:
    class Private;
    std::shared_ptr<Private> d;
};

} // namespace Kleo

#endif // SRC_VIEW_CERTIFYWIDGET_H
