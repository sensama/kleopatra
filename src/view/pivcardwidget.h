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
    struct KeyWidgets {
        QLabel *keyGrip = nullptr;
        QLabel *keyAlgorithm = nullptr;
        QPushButton *generateButton = nullptr;
        QPushButton *writeCertificateButton = nullptr;
        QPushButton *importCertificateButton = nullptr;
        QPushButton *writeKeyButton = nullptr;
    };

    KeyWidgets createKeyWidgets(const std::string &keyRef);
    void updateKey(const std::string &keyRef, const SmartCard::PIVCard *card, const KeyWidgets &widgets);
    void generateKey(const std::string &keyref);
    void writeCertificateToCard(const std::string &keyref);
    void importCertificateFromCard(const std::string &keyref);
    void writeKeyToCard(const std::string &keyref);
    void changePin(const std::string &keyRef);
    void setAdminKey();

private:
    std::string mCardSerialNumber;
    QLabel *mSerialNumber = nullptr;
    QLabel *mVersionLabel = nullptr;
    KeyWidgets mPIVAuthenticationKey;
    KeyWidgets mCardAuthenticationKey;
    KeyWidgets mDigitalSignatureKey;
    KeyWidgets mKeyManagementKey;
};
} // namespace Kleo

#endif // VIEW_PIVCARDWIDGET_H
