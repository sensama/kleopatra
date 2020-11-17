/*  view/pivcardwiget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef VIEW_PIVCARDWIDGET_H
#define VIEW_PIVCARDWIDGET_H

#include <QMap>
#include <QWidget>

#include <gpgme++/error.h>

class QGridLayout;
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

    struct KeyWidgets {
        QLabel *keyGrip = nullptr;
        QLabel *keyAlgorithm = nullptr;
        QLabel *certificateInfo = nullptr;
        QPushButton *generateButton = nullptr;
        QPushButton *writeCertificateButton = nullptr;
        QPushButton *importCertificateButton = nullptr;
        QPushButton *writeKeyButton = nullptr;
    };

private:
    KeyWidgets createKeyWidgets(const std::string &keyRef);
    void updateKeyWidgets(const std::string &keyRef, const SmartCard::PIVCard *card);
    void generateKey(const std::string &keyref);
    void writeCertificateToCard(const std::string &keyref);
    void importCertificateFromCard(const std::string &keyref);
    void writeKeyToCard(const std::string &keyref);
    void createKeyFromCardKeys();
    void changePin(const std::string &keyRef);
    void setAdminKey();

private:
    std::string mCardSerialNumber;
    QLabel *mSerialNumber = nullptr;
    QLabel *mVersionLabel = nullptr;
    QPushButton *mKeyForCardKeysButton = nullptr;
    QMap<std::string, KeyWidgets> mKeyWidgets;
};
} // namespace Kleo

#endif // VIEW_PIVCARDWIDGET_H
