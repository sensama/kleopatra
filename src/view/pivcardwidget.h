/*  view/pivcardwiget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef VIEW_PIVCARDWIDGET_H
#define VIEW_PIVCARDWIDGET_H

#include <QWidget>

#include <gpgme++/error.h>

class QLabel;
class QPushButton;

namespace Kleo
{

namespace SmartCard
{
class PIVCard;
} // namespace SmartCard

class PIVCardWidget: public QWidget
{
    Q_OBJECT
public:
    explicit PIVCardWidget(QWidget *parent = nullptr);
    ~PIVCardWidget();

    void setCard(const SmartCard::PIVCard* card);

private:
    void updateKey(QLabel *label, QPushButton *button, const std::string &fpr);
    void generateKey(const QByteArray &keyref);

private Q_SLOTS:
    void generatePIVAuthenticationKey();
    void generateCardAuthenticationKey();
    void generateDigitalSignatureKey();
    void generateKeyManagementKey();

private:
    QLabel *mSerialNumber = nullptr,
           *mVersionLabel = nullptr,
           *mPIVAuthenticationKey = nullptr,
           *mCardAuthenticationKey = nullptr,
           *mDigitalSignatureKey = nullptr,
           *mKeyManagementKey = nullptr;
    QPushButton *mGeneratePIVAuthenticationKeyBtn = nullptr,
                *mGenerateCardAuthenticationKeyBtn = nullptr,
                *mGenerateDigitalSignatureKeyBtn = nullptr,
                *mGenerateKeyManagementKeyBtn = nullptr;
    bool mCardIsEmpty = false;
};
} // namespace Kleo

#endif // VIEW_PIVCARDWIDGET_H
