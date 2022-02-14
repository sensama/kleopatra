/* -*- mode: c++; c-basic-offset:4 -*-
    smartcard/readerstatus.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QMetaType>

#include "card.h"

#include <vector>
#include <memory>

namespace GpgME
{
class AssuanTransaction;
class Context;
class Error;
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
    ~ReaderStatus() override;

    static const ReaderStatus *instance();
    static ReaderStatus *mutableInstance();

    void startSimpleTransaction(const std::shared_ptr<Card> &card, const QByteArray &cmd, QObject *receiver, const char *slot);
    void startTransaction(const std::shared_ptr<Card> &card, const QByteArray &cmd, QObject *receiver, const char *slot,
                          std::unique_ptr<GpgME::AssuanTransaction> transaction);

    Card::Status cardStatus(unsigned int slot) const;
    std::string firstCardWithNullPin() const;
    bool anyCardCanLearnKeys() const;

    std::vector<std::shared_ptr<Card> > getCards() const;

    std::shared_ptr<Card> getCard(const std::string &serialNumber, const std::string &appName) const;

    template <typename T>
    std::shared_ptr<T> getCard(const std::string &serialNumber) const
    {
        return std::dynamic_pointer_cast<T>(getCard(serialNumber, T::AppName));
    }

    static std::string switchCard(std::shared_ptr<GpgME::Context> &ctx, const std::string &serialNumber, GpgME::Error &err);
    static std::string switchApp(std::shared_ptr<GpgME::Context> &ctx, const std::string &serialNumber,
                                 const std::string &appName, GpgME::Error &err);
    static GpgME::Error switchCardAndApp(const std::string &serialNumber, const std::string &appName);

public Q_SLOTS:
    void updateStatus();
    void startMonitoring();

Q_SIGNALS:
    void firstCardWithNullPinChanged(const std::string &serialNumber);
    void anyCardCanLearnKeysChanged(bool);
    void cardAdded(const std::string &serialNumber, const std::string &appName);
    void cardChanged(const std::string &serialNumber, const std::string &appName);
    void cardRemoved(const std::string &serialNumber, const std::string &appName);
    void startOfGpgAgentRequested();

private:
    class Private;
    std::shared_ptr<Private> d;
};

} // namespace SmartCard
} // namespace Kleo

Q_DECLARE_METATYPE(Kleo::SmartCard::Card::Status)

