/*  view/netkeywidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef VIEW_NETKEYWIDGET_H
#define VIEW_NETKEYWIDGET_H

#include <QWidget>
#include <gpgme++/error.h>

#include <string>

class QLabel;
class QPushButton;
class QScrollArea;

namespace Kleo
{
class NullPinWidget;
class KeyTreeView;

namespace SmartCard
{
class NetKeyCard;
} // namespace SmartCard

class NetKeyWidget: public QWidget
{
    Q_OBJECT
public:
    explicit NetKeyWidget(QWidget *parent = nullptr);
    ~NetKeyWidget();

    void setCard(const SmartCard::NetKeyCard *card);

private:
    void handleResult(const GpgME::Error &err, QPushButton *btn);
    void doChangePin(bool sigG);
    void createKeyFromCardKeys();

private Q_SLOTS:
    void setSigGPinSettingResult(const GpgME::Error &err);
    void setNksPinSettingResult(const GpgME::Error &err);

private:
    std::string mSerialNumber;
    QLabel *mSerialNumberLabel = nullptr,
           *mVersionLabel = nullptr,
           *mLearnKeysLabel = nullptr,
           *mErrorLabel = nullptr;
    NullPinWidget *mNullPinWidget = nullptr;
    QPushButton *mLearnKeysBtn = nullptr,
                *mKeyForCardKeysButton = nullptr,
                *mChangeNKSPINBtn = nullptr,
                *mChangeSigGPINBtn = nullptr;
    KeyTreeView *mTreeView = nullptr;
    QScrollArea *mArea = nullptr;
};
} // namespace Kleo

#endif // VIEW_NETKEYWIDGET_H
