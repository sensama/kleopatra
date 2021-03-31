/*  view/pgpcardwiget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "commands/changepincommand.h"

#include <QMap>
#include <QWidget>

#include <gpgme++/error.h>

#include <string>

class QLabel;
class QPushButton;

namespace Kleo
{
class GenCardKeyDialog;

namespace SmartCard
{
struct KeyPairInfo;
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

    struct KeyWidgets {
        std::string keyGrip;
        QLabel *keyFingerprint = nullptr;
        QPushButton *createCSRButton = nullptr;
    };

public Q_SLOTS:
    void genkeyRequested();
    void changeNameRequested();
    void changeNameResult(const GpgME::Error &err);
    void changeUrlRequested();
    void changeUrlResult(const GpgME::Error &err);
    void createKeyFromCardKeys();
    void createCSR(const std::string &keyref);

private:
    KeyWidgets createKeyWidgets(const SmartCard::KeyPairInfo &keyInfo);
    void updateKeyWidgets(const std::string &keyRef, const SmartCard::OpenPGPCard *card);
    void doChangePin(const std::string &keyRef, Commands::ChangePinCommand::ChangePinMode mode = Commands::ChangePinCommand::NormalMode);

private:
    QLabel *mSerialNumber = nullptr,
           *mCardHolderLabel = nullptr,
           *mVersionLabel = nullptr,
           *mUrlLabel = nullptr;
    QPushButton *mKeyForCardKeysButton = nullptr;
    QMap<std::string, KeyWidgets> mKeyWidgets;
    QString mUrl;
    bool mCardIsEmpty = false;
    bool mIs21 = false;
    std::string mRealSerial;
};
} // namespace Kleo

