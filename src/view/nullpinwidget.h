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

    void setSigGVisible(bool val);
    void setNKSVisible(bool val);

private:
    void doChangePin(bool sigG);
    void handleResult(const GpgME::Error &err, QPushButton *btn);

private Q_SLOTS:
    void setSigGPinSettingResult(const GpgME::Error &err);
    void setNksPinSettingResult(const GpgME::Error &err);

private:
    QPushButton *mNKSBtn,
                *mSigGBtn;
};

} // namespace Kleo

#endif // VIEW_NULLPINWIDGET_H
