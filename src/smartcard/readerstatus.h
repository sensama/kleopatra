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

#include "kleopatra_debug.h"

namespace GpgME
{
class AssuanTransaction;
}

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

    void startSimpleTransaction(const std::shared_ptr<Card> &card, const QByteArray &cmd, QObject *receiver, const char *slot);
    void startTransaction(const std::shared_ptr<Card> &card, const QByteArray &cmd, QObject *receiver, const char *slot,
                          std::unique_ptr<GpgME::AssuanTransaction> transaction);

    Card::Status cardStatus(unsigned int slot) const;
    std::string firstCardWithNullPin() const;
    bool anyCardCanLearnKeys() const;

    std::vector<std::shared_ptr<Card> > getCards() const;

    template <typename T>
    std::shared_ptr<T> getCard(const std::string &serialNumber) const
    {
        for (const auto &card: getCards()) {
            if (card->serialNumber() == serialNumber) {
                qCDebug(KLEOPATRA_LOG) << "ReaderStatus::getCard() - Found card with serial number" << QString::fromStdString(serialNumber);
                return std::dynamic_pointer_cast<T>(card);
            }
        }
        qCDebug(KLEOPATRA_LOG) << "ReaderStatus::getCard() - Did not find card with serial number" << QString::fromStdString(serialNumber);
        return std::shared_ptr<T>();
    }

public Q_SLOTS:
    void updateStatus();
    void startMonitoring();

Q_SIGNALS:
    void firstCardWithNullPinChanged(const std::string &serialNumber);
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
