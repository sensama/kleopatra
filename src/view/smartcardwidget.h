/*  view/smartcardwidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2024 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QWidget>

#include <memory>
#include <string>

class QGridLayout;
class QVBoxLayout;

namespace GpgME
{
class Key;
}

namespace Kleo
{
class CardKeysView;
class InfoField;
}
namespace Kleo::SmartCard
{
enum class AppType;
class Card;
}

class SmartCardWidget : public QWidget
{
    Q_OBJECT
public:
    SmartCardWidget(QWidget *parent = nullptr);
    ~SmartCardWidget() override;

    void setCard(const Kleo::SmartCard::Card *card);

    Kleo::SmartCard::AppType cardType() const;
    std::string serialNumber() const;
    std::string currentCardSlot() const;
    GpgME::Key currentCertificate() const;

    void updateActions();

protected:
    QVBoxLayout *mContentLayout = nullptr;
    QGridLayout *mInfoGridLayout = nullptr;

private:
    std::unique_ptr<const Kleo::SmartCard::Card> mCard;

    std::unique_ptr<Kleo::InfoField> mCardTypeField;
    std::unique_ptr<Kleo::InfoField> mSerialNumberField;

protected:
    Kleo::CardKeysView *mCardKeysView = nullptr;
};
