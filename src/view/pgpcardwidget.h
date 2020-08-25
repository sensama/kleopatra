/*  view/pgpcardwiget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef VIEW_PGPCARDWIDGET_H
#define VIEW_PGPCARDWIDGET_H

#include <QWidget>
#include <gpgme++/error.h>

#include <string>

class QLabel;

namespace Kleo
{
class GenCardKeyDialog;

namespace SmartCard
{
class OpenPGPCard;
} // namespace SmartCard

class PGPCardWidget: public QWidget
{
    Q_OBJECT
public:
    explicit PGPCardWidget(QWidget *parent = nullptr);

    void setCard(const SmartCard::OpenPGPCard* card);
    void doGenKey(GenCardKeyDialog *dlg);
    void genKeyDone(const GpgME::Error &err, const std::string &backup);

public Q_SLOTS:
    void genkeyRequested();
    void changeNameRequested();
    void changeNameResult(const GpgME::Error &err);
    void changeUrlRequested();
    void changeUrlResult(const GpgME::Error &err);

private:
    void doChangePin(const std::string &keyRef);
    void updateKey(QLabel *label, const std::string &fpr);
    QLabel *mSerialNumber = nullptr,
           *mCardHolderLabel = nullptr,
           *mVersionLabel = nullptr,
           *mSigningKey = nullptr,
           *mEncryptionKey = nullptr,
           *mAuthKey = nullptr,
           *mUrlLabel = nullptr;
    QString mUrl;
    bool mCardIsEmpty = false;
    bool mIs21 = false;
    std::string mRealSerial;
};
} // namespace Kleo

#endif // VIEW_PGPCARDWIDGET_H
