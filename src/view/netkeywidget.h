/*  view/netkeywidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QWidget>
#include <gpgme++/error.h>

#include <string>
#include <vector>

class QLabel;
class QPushButton;
class QScrollArea;

namespace GpgME
{
class Key;
}

namespace Kleo
{
class NullPinWidget;
class KeyTreeView;

namespace SmartCard
{
class NetKeyCard;
} // namespace SmartCard

class NetKeyWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NetKeyWidget(QWidget *parent = nullptr);
    ~NetKeyWidget() override;

    void setCard(const SmartCard::NetKeyCard *card);

private:
    void loadCertificates();
    void learnCard();
    void doChangePin(const std::string &keyRef);
    void createKeyFromCardKeys();
    void createCSR();

private:
    std::string mSerialNumber;
    std::vector<GpgME::Key> mCertificates;

    QLabel *mSerialNumberLabel = nullptr;
    QLabel *mVersionLabel = nullptr;
    QLabel *mErrorLabel = nullptr;
    NullPinWidget *mNullPinWidget = nullptr;
    QPushButton *mKeyForCardKeysButton = nullptr;
    QPushButton *mCreateCSRButton = nullptr;
    QPushButton *mChangeNKSPINBtn = nullptr;
    QPushButton *mChangeSigGPINBtn = nullptr;
    KeyTreeView *mTreeView = nullptr;
    QScrollArea *mArea = nullptr;
};
} // namespace Kleo
