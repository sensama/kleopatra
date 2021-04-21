/*  view/p15cardwiget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Andre Heinecke <aheinecke@g10code.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QWidget>
#include <QLabel>

class QLabel;

namespace Kleo
{

namespace SmartCard
{
struct KeyPairInfo;
class P15Card;
} // namespace SmartCard

class P15CardWidget: public QWidget
{
    Q_OBJECT
public:
    explicit P15CardWidget(QWidget *parent = nullptr);
    ~P15CardWidget();

    void setCard(const SmartCard::P15Card* card);

private:
    std::string mCardSerialNumber;
    QLabel *mSerialNumber = nullptr;
    QLabel *mVersionLabel = nullptr;
    QLabel *mSigFprLabel  = nullptr;
    QLabel *mEncFprLabel  = nullptr;
};

}
