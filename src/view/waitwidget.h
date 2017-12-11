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

#ifndef KLEO_WAITWIDGET_H
#define KLEO_WAITWIDGET_H

#include <QWidget>

class QLabel;

namespace Kleo
{

class WaitWidget: public QWidget
{
    Q_OBJECT

public:
    explicit WaitWidget(QWidget *parent = nullptr);
    ~WaitWidget();

    void setText(const QString &text);

private:
    QLabel *mLabel;
};

} // namespace Kleo
#endif
