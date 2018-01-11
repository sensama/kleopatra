/*  Copyright (c) 2016 Klarälvdalens Datakonsult AB

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

#ifndef KLEO_CERTIFICATEDETAILS_WIDGET_H
#define KLEO_CERTIFICATEDETAILS_WIDGET_H

#include <QWidget>
#include <QDialog>

#include <vector>

namespace GpgME {
class Key;
class Error;
class KeyListResult;
}

class CertificateDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CertificateDetailsWidget(QWidget *parent = nullptr);
    ~CertificateDetailsWidget();

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    class Private;
    const QScopedPointer<Private> d;

    // Windows QGpgME new style connect problem makes this necessary.
    Q_PRIVATE_SLOT(d, void keyListDone(const GpgME::KeyListResult &,
                   const std::vector<GpgME::Key> &, const QString &,
                   const GpgME::Error &))
};


class CertificateDetailsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CertificateDetailsDialog(QWidget *parent = nullptr);
    ~CertificateDetailsDialog();

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    void readConfig();
    void writeConfig();
};

#endif
