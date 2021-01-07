#ifndef VIEW_NULLPINWIDGET_H
#define VIEW_NULLPINWIDGET_H
/*  view/nullpinwidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QWidget>

class QPushButton;

namespace GpgME
{
    class Error;
} // namespace GpgME

namespace Kleo
{

class NullPinWidget: public QWidget
{
    Q_OBJECT
public:
    explicit NullPinWidget(QWidget *parent = nullptr);

    void setSerialNumber(const std::string &serialNumber);
    void setSigGVisible(bool val);
    void setNKSVisible(bool val);

private:
    void doChangePin(const std::string &keyRef);

private:
    std::string mSerialNumber;
    QPushButton *mNKSBtn,
                *mSigGBtn;
};

} // namespace Kleo

#endif // VIEW_NULLPINWIDGET_H
