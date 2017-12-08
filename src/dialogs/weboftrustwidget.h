/*  Copyright (c) 2017 Intevation GmbH

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
*/

#ifndef KLEO_WEBOFTRUSTWIDGET_H
#define KLEO_WEBOFTRUSTWIDGET_H

#include <QWidget>

namespace GpgME {
class Key;
class KeyListResult;
}

namespace Kleo {

class WebOfTrustWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WebOfTrustWidget(QWidget *parent = nullptr);
    ~WebOfTrustWidget();

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private Q_SLOTS:
    void signatureListingNextKey(const GpgME::Key &key);
    void signatureListingDone(const GpgME::KeyListResult &result);

private:
    class Private;
    const QScopedPointer<Private> d;
};
} // namespace Kleo
#endif

