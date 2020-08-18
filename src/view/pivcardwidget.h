/*  view/pivcardwiget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef VIEW_PIVCARDWIDGET_H
#define VIEW_PIVCARDWIDGET_H

#include <QWidget>

class QLabel;

namespace Kleo
{
class GenCardKeyDialog;

namespace SmartCard
{
class PIVCard;
} // namespace SmartCard

class PIVCardWidget: public QWidget
{
    Q_OBJECT
public:
    explicit PIVCardWidget(QWidget *parent = nullptr);

    void setCard(const SmartCard::PIVCard* card);

private:
    void updateKey(QLabel *label, const std::string &fpr);

    QLabel *mSerialNumber = nullptr,
           *mVersionLabel = nullptr,
           *mPivAuthenticationKey = nullptr,
           *mCardAuthenticationKey = nullptr,
           *mSigningKey = nullptr,
           *mEncryptionKey = nullptr;
    bool mCardIsEmpty = false;
};
} // namespace Kleo

#endif // VIEW_PIVCARDWIDGET_H
