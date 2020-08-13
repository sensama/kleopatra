/* -*- mode: c++; c-basic-offset:4 -*-
    smartcard/readerstatus.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA__SMARTCARD__READERSTATUS_H___
#define __KLEOPATRA__SMARTCARD__READERSTATUS_H___

#include <QObject>
#include <QMetaType>

#include "card.h"

#include <vector>
#include <memory>

namespace Kleo
{
namespace SmartCard
{

class ReaderStatus : public QObject
{
    Q_OBJECT
public:
    explicit ReaderStatus(QObject *parent = nullptr);
    ~ReaderStatus();

    static const ReaderStatus *instance();
    static ReaderStatus *mutableInstance();

    void startSimpleTransaction(const QByteArray &cmd, QObject *receiver, const char *slot);

    Card::Status cardStatus(unsigned int slot) const;
    bool anyCardHasNullPin() const;
    bool anyCardCanLearnKeys() const;

    std::vector<Card::PinState> pinStates(unsigned int slot) const;

    std::vector<std::shared_ptr<Card> > getCards() const;

public Q_SLOTS:
    void updateStatus();
    void startMonitoring();

Q_SIGNALS:
    void anyCardHasNullPinChanged(bool);
    void anyCardCanLearnKeysChanged(bool);
    void cardChanged(unsigned int slot);

private:
    class Private;
    std::shared_ptr<Private> d;
};

} // namespace SmartCard
} // namespace Kleo

Q_DECLARE_METATYPE(Kleo::SmartCard::Card::Status)

#endif /* __KLEOPATRA__SMARTCARD__READERSTATUS_H___ */
