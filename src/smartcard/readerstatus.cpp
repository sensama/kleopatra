/* -*- mode: c++; c-basic-offset:4 -*-
    smartcard/readerstatus.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2009 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config-kleopatra.h>

#include "readerstatus.h"

#include <utils/gnupg-helper.h>

#include <Libkleo/FileSystemWatcher>
#include <Libkleo/Stl_Util>

#include <gpgme++/context.h>
#include <gpgme++/defaultassuantransaction.h>
#include <gpgme++/key.h>

#include <gpg-error.h>

#include "kleopatra_debug.h"
#include "openpgpcard.h"
#include "netkeycard.h"

#include <QStringList>
#include <QFileInfo>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QPointer>

#include <memory>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <iterator>
#include <utility>
#include <cstdlib>

#include "utils/kdtoolsglobal.h"

using namespace Kleo;
using namespace Kleo::SmartCard;
using namespace GpgME;

static ReaderStatus *self = nullptr;


static const char *flags[] = {
    "NOCARD",
    "PRESENT",
    "ACTIVE",
    "USABLE",
};
static_assert(sizeof flags / sizeof * flags == Card::_NumScdStates, "");

static const char *prettyFlags[] = {
    "NoCard",
    "CardPresent",
    "CardActive",
    "CardUsable",
    "CardError",
};
static_assert(sizeof prettyFlags / sizeof * prettyFlags == Card::NumStates, "");

#if 0
We need this once we have support for multiple readers in scdaemons
interface.
static unsigned int parseFileName(const QString &fileName, bool *ok)
{
    QRegExp rx(QLatin1String("reader_(\\d+)\\.status"));
    if (ok) {
        *ok = false;
    }
    if (rx.exactMatch(QFileInfo(fileName).fileName())) {
        return rx.cap(1).toUInt(ok, 10);
    }
    return 0;
}
#endif

Q_DECLARE_METATYPE(GpgME::Error)

namespace
{
static QDebug operator<<(QDebug s, const std::vector< std::pair<std::string, std::string> > &v)
{
    typedef std::pair<std::string, std::string> pair;
    s << '(';
    for (const pair &p : v) {
        s << "status(" << QString::fromStdString(p.first) << ") =" << QString::fromStdString(p.second) << '\n';
    }
    return s << ')';
}

static const char *app_types[] = {
    "_", // will hopefully never be used as an app-type :)
    "openpgp",
    "nks",
    "p15",
    "dinsig",
    "geldkarte",
};
static_assert(sizeof app_types / sizeof * app_types == Card::NumAppTypes, "");

static Card::AppType parse_app_type(const std::string &s)
{
    qCDebug(KLEOPATRA_LOG) << "parse_app_type(" << s.c_str() << ")";
    const char **it = std::find_if(std::begin(app_types), std::end(app_types),
                                [&s](const char *type) {
                                    return ::strcasecmp(s.c_str(), type) == 0;
                                });
    if (it == std::end(app_types)) {
        qCDebug(KLEOPATRA_LOG) << "App type not found";
        return Card::UnknownApplication;
    }
    return static_cast<Card::AppType>(it - std::begin(app_types));

}

static int parse_app_version(const std::string &s)
{
    return std::atoi(s.c_str());
}

static Card::PinState parse_pin_state(const QString &s)
{
    bool ok;
    int i = s.toInt(&ok);
    if (!ok) {
        qCDebug(KLEOPATRA_LOG) << "Failed to parse pin state" << s;
        return Card::UnknownPinState;
    }
    switch (i) {
    case -4: return Card::NullPin;
    case -3: return Card::PinBlocked;
    case -2: return Card::NoPin;
    case -1: return Card::UnknownPinState;
    default:
        if (i < 0) {
            return Card::UnknownPinState;
        } else {
            return Card::PinOk;
        }
    }
}

static std::unique_ptr<DefaultAssuanTransaction> gpgagent_transact(std::shared_ptr<Context> &gpgAgent, const char *command, Error &err)
{
    qCDebug(KLEOPATRA_LOG) << "gpgagent_transact(" << command << ")";
    err = gpgAgent->assuanTransact(command);
    if (err.code()) {
        qCDebug(KLEOPATRA_LOG) << "gpgagent_transact(" << command << "):" << QString::fromLocal8Bit(err.asString());
        if (err.code() >= GPG_ERR_ASS_GENERAL && err.code() <= GPG_ERR_ASS_UNKNOWN_INQUIRE) {
            qCDebug(KLEOPATRA_LOG) << "Assuan problem, killing context";
            gpgAgent.reset();
        }
        return std::unique_ptr<DefaultAssuanTransaction>();
    }
    std::unique_ptr<AssuanTransaction> t = gpgAgent->takeLastAssuanTransaction();
    return std::unique_ptr<DefaultAssuanTransaction>(dynamic_cast<DefaultAssuanTransaction*>(t.release()));
}

const std::vector< std::pair<std::string, std::string> > gpgagent_statuslines(std::shared_ptr<Context> gpgAgent, const char *what, Error &err)
{
    const std::unique_ptr<DefaultAssuanTransaction> t = gpgagent_transact(gpgAgent, what, err);
    if (t.get()) {
        qCDebug(KLEOPATRA_LOG) << "agent_getattr_status(" << what << "): got" << t->statusLines();
        return t->statusLines();
    } else {
        qCDebug(KLEOPATRA_LOG) << "agent_getattr_status(" << what << "): t == NULL";
        return std::vector<std::pair<std::string, std::string> >();
    }

}

static const std::string gpgagent_status(const std::shared_ptr<Context> &gpgAgent, const char *what, Error &err)
{
    const auto lines = gpgagent_statuslines (gpgAgent, what, err);
    // The status is only the last attribute
    // e.g. for SCD SERIALNO it would only be "SERIALNO" and for SCD GETATTR FOO
    // it would only be FOO
    const char *p = strrchr(what, ' ');
    const char *needle = (p + 1) ? (p + 1) : what;
    for (const auto &pair: lines) {
        if (pair.first == needle) {
            return pair.second;
        }
    }
    return std::string();
}

static const std::string scd_getattr_status(std::shared_ptr<Context> &gpgAgent, const char *what, Error &err)
{
    std::string cmd = "SCD GETATTR ";
    cmd += what;
    return gpgagent_status(gpgAgent, cmd.c_str(), err);
}

static void handle_openpgp_card(std::shared_ptr<Card> &ci, std::shared_ptr<Context> &gpg_agent)
{
    Error err;
    auto ret = new OpenPGPCard();
    ret->setSerialNumber(ci->serialNumber());

    const auto info = gpgagent_statuslines(gpg_agent, "SCD LEARN --keypairinfo", err);
    if (err.code()) {
        ci->setStatus(Card::CardError);
        return;
    }
    ret->setKeyPairInfo(info);
    ci.reset(ret);
}

static void handle_netkey_card(std::shared_ptr<Card> &ci, std::shared_ptr<Context> &gpg_agent)
{
    Error err;
    auto nkCard = new NetKeyCard();
    nkCard->setSerialNumber(ci->serialNumber());
    ci.reset(nkCard);

    ci->setAppVersion(parse_app_version(scd_getattr_status(gpg_agent, "NKS-VERSION", err)));

    if (err.code()) {
        qCDebug(KLEOPATRA_LOG) << "NKS-VERSION resulted in error" << err.asString();
        ci->setErrorMsg(QStringLiteral ("NKS-VERSION failed: ") + QString::fromUtf8(err.asString()));
        return;
    }

    if (ci->appVersion() != 3) {
        qCDebug(KLEOPATRA_LOG) << "not a NetKey v3 card, giving up. Version:" << ci->appVersion();
        ci->setErrorMsg(QStringLiteral("NetKey v%1 cards are not supported.").arg(ci->appVersion()));
        return;
    }
    // the following only works for NKS v3...
    const auto chvStatus = QString::fromStdString(
            scd_getattr_status(gpg_agent, "CHV-STATUS", err)).split(QLatin1Char(' '));
    if (err.code()) {
        qCDebug(KLEOPATRA_LOG) << "no CHV-STATUS" << err.asString();
        ci->setErrorMsg(QStringLiteral ("CHV-Status failed: ") + QString::fromUtf8(err.asString()));
        return;
    }

    std::vector<Card::PinState> states;
    states.reserve(chvStatus.count());
    // CHV Status for NKS v3 is
    // Pin1 (Normal pin) Pin2 (Normal PUK)
    // SigG1 SigG PUK.
    int num = 0;
    for (const auto &state: chvStatus) {
        const auto parsed = parse_pin_state (state);
        states.push_back(parsed);
        if (parsed == Card::NullPin) {
            if (num == 0) {
                ci->setHasNullPin(true);
            }
        }
        ++num;
    }
    nkCard->setPinStates(states);

    // check for keys to learn:
    const std::unique_ptr<DefaultAssuanTransaction> result = gpgagent_transact(gpg_agent, "SCD LEARN --keypairinfo", err);
    if (err.code() || !result.get()) {
        if (err) {
            ci->setErrorMsg(QString::fromLatin1(err.asString()));
        } else {
            ci->setErrorMsg(QStringLiteral("Invalid internal state. No result."));
        }
        return;
    }
    const std::vector<std::string> keyPairInfos = result->statusLine("KEYPAIRINFO");
    if (keyPairInfos.empty()) {
        return;
    }
    nkCard->setKeyPairInfo(keyPairInfos);
}

static std::shared_ptr<Card> get_card_status(unsigned int slot, std::shared_ptr<Context> &gpg_agent)
{
    qCDebug(KLEOPATRA_LOG) << "get_card_status(" << slot << ',' << gpg_agent.get() << ')';
    auto ci = std::shared_ptr<Card> (new Card());
    if (slot != 0 || !gpg_agent) {
        // In the future scdaemon should support multiple slots but
        // not yet (2.1.18)
        return ci;
    }

    Error err;
    ci->setSerialNumber(gpgagent_status(gpg_agent, "SCD SERIALNO", err));
    if (err.code() == GPG_ERR_CARD_NOT_PRESENT || err.code() == GPG_ERR_CARD_REMOVED) {
        ci->setStatus(Card::NoCard);
        return ci;
    }
    if (err.code()) {
        ci->setStatus(Card::CardError);
        return ci;
    }
    ci->setStatus(Card::CardPresent);

    const auto verbatimType = scd_getattr_status(gpg_agent, "APPTYPE", err);
    ci->setAppType(parse_app_type(verbatimType));
    if (err.code()) {
        return ci;
    }

    // Handle different card types
    if (ci->appType() == Card::NksApplication) {
        qCDebug(KLEOPATRA_LOG) << "get_card_status: found Netkey card" << ci->serialNumber().c_str() << "end";
        handle_netkey_card(ci, gpg_agent);
        return ci;
    } else if (ci->appType() == Card::OpenPGPApplication) {
        qCDebug(KLEOPATRA_LOG) << "get_card_status: found OpenPGP card" << ci->serialNumber().c_str() << "end";
        handle_openpgp_card(ci, gpg_agent);
        return ci;
    } else {
        qCDebug(KLEOPATRA_LOG) << "get_card_status: unhandled application:" << verbatimType.c_str();
        return ci;
    }

    return ci;
}

static std::vector<std::shared_ptr<Card> > update_cardinfo(std::shared_ptr<Context> &gpgAgent)
{
    // Multiple smartcard readers are only supported internally by gnupg
    // but not by scdaemon (Status gnupg 2.1.18)
    // We still pretend that there can be multiple cards inserted
    // at once but we don't handle it yet.
    const auto ci = get_card_status(0, gpgAgent);
    return std::vector<std::shared_ptr<Card> >(1, ci);
}
} // namespace

struct Transaction {
    QByteArray command;
    QPointer<QObject> receiver;
    const char *slot;
};

static const Transaction updateTransaction = { "__update__", nullptr, nullptr };
static const Transaction quitTransaction   = { "__quit__",   nullptr, nullptr };

namespace
{
class ReaderStatusThread : public QThread
{
    Q_OBJECT
public:
    explicit ReaderStatusThread(QObject *parent = nullptr)
        : QThread(parent),
          m_gnupgHomePath(Kleo::gnupgHomeDirectory()),
          m_transactions(1, updateTransaction)   // force initial scan
    {
        connect(this, &ReaderStatusThread::oneTransactionFinished,
                this, &ReaderStatusThread::slotOneTransactionFinished);
    }

    std::vector<std::shared_ptr<Card> > cardInfos() const
    {
        const QMutexLocker locker(&m_mutex);
        return m_cardInfos;
    }

    Card::Status cardStatus(unsigned int slot) const
    {
        const QMutexLocker locker(&m_mutex);
        if (slot < m_cardInfos.size()) {
            return m_cardInfos[slot]->status();
        } else {
            return Card::NoCard;
        }
    }

    void addTransaction(const Transaction &t)
    {
        const QMutexLocker locker(&m_mutex);
        m_transactions.push_back(t);
        m_waitForTransactions.wakeOne();
    }

Q_SIGNALS:
    void anyCardHasNullPinChanged(bool);
    void anyCardCanLearnKeysChanged(bool);
    void cardChanged(unsigned int);
    void oneTransactionFinished(const GpgME::Error &err);

public Q_SLOTS:
    void ping()
    {
        qCDebug(KLEOPATRA_LOG) << "ReaderStatusThread[GUI]::ping()";
        addTransaction(updateTransaction);
    }

    void stop()
    {
        const QMutexLocker locker(&m_mutex);
        m_transactions.push_front(quitTransaction);
        m_waitForTransactions.wakeOne();
    }

private Q_SLOTS:
    void slotOneTransactionFinished(const GpgME::Error &err)
    {
        std::list<Transaction> ft;
        KDAB_SYNCHRONIZED(m_mutex)
        ft.splice(ft.begin(), m_finishedTransactions);
        Q_FOREACH (const Transaction &t, ft)
            if (t.receiver && t.slot && *t.slot) {
                QMetaObject::invokeMethod(t.receiver, t.slot, Qt::DirectConnection, Q_ARG(GpgME::Error, err));
            }
    }

private:
    void run() override {
        while (true) {
            std::shared_ptr<Context> gpgAgent;

            QByteArray command;
            bool nullSlot = false;
            std::list<Transaction> item;
            std::vector<std::shared_ptr<Card> > oldCards;

            Error err;
            std::unique_ptr<Context> c = Context::createForEngine(AssuanEngine, &err);
            if (err.code() == GPG_ERR_NOT_SUPPORTED) {
                return;
            }
            gpgAgent = std::shared_ptr<Context>(c.release());

            KDAB_SYNCHRONIZED(m_mutex) {

                while (m_transactions.empty()) {
                    // go to sleep waiting for more work:
                    qCDebug(KLEOPATRA_LOG) << "ReaderStatusThread[2nd]: waiting for commands";
                    m_waitForTransactions.wait(&m_mutex);
                }

                // splice off the first transaction without
                // copying, so we own it without really importing
                // it into this thread (the QPointer isn't
                // thread-safe):
                item.splice(item.end(),
                            m_transactions, m_transactions.begin());

                // make local copies of the interesting stuff so
                // we can release the mutex again:
                command = item.front().command;
                nullSlot = !item.front().slot;
                oldCards = m_cardInfos;
            }

            qCDebug(KLEOPATRA_LOG) << "ReaderStatusThread[2nd]: new iteration command=" << command << " ; nullSlot=" << nullSlot;
            // now, let's see what we got:
            if (nullSlot && command == quitTransaction.command) {
                return;    // quit
            }

            if ((nullSlot && command == updateTransaction.command)) {

                std::vector<std::shared_ptr<Card> > newCards = update_cardinfo(gpgAgent);

                newCards.resize(std::max(newCards.size(), oldCards.size()));
                oldCards.resize(std::max(newCards.size(), oldCards.size()));

                KDAB_SYNCHRONIZED(m_mutex)
                m_cardInfos = newCards;

                std::vector<std::shared_ptr<Card> >::const_iterator
                nit = newCards.begin(), nend = newCards.end(),
                oit = oldCards.begin(), oend = oldCards.end();

                unsigned int idx = 0;
                bool anyLC = false;
                bool anyNP = false;
                bool anyError = false;
                while (nit != nend && oit != oend) {
                    const auto optr = (*oit).get();
                    const auto nptr = (*nit).get();
                    if ((optr && !nptr) || (!optr && nptr) || (optr && nptr && *optr != *nptr)) {
                        qCDebug(KLEOPATRA_LOG) << "ReaderStatusThread[2nd]: slot" << idx << ": card Changed";
                        Q_EMIT cardChanged(idx);
                    }
                    if ((*nit)->canLearnKeys()) {
                        anyLC = true;
                    }
                    if ((*nit)->hasNullPin()) {
                        anyNP = true;
                    }
                    if ((*nit)->status() == Card::CardError) {
                        anyError = true;
                    }
                    ++nit;
                    ++oit;
                    ++idx;
                }

                Q_EMIT anyCardHasNullPinChanged(anyNP);
                Q_EMIT anyCardCanLearnKeysChanged(anyLC);

                if (anyError) {
                    gpgAgent.reset();
                }
            } else {
                GpgME::Error err;
                (void)gpgagent_transact(gpgAgent, command.constData(), err);

                KDAB_SYNCHRONIZED(m_mutex)
                // splice 'item' into m_finishedTransactions:
                m_finishedTransactions.splice(m_finishedTransactions.end(), item);

                Q_EMIT oneTransactionFinished(err);
            }
        }
    }

private:
    mutable QMutex m_mutex;
    QWaitCondition m_waitForTransactions;
    const QString m_gnupgHomePath;
    // protected by m_mutex:
    std::vector<std::shared_ptr<Card> > m_cardInfos;
    std::list<Transaction> m_transactions, m_finishedTransactions;
};

}

class ReaderStatus::Private : ReaderStatusThread
{
    friend class Kleo::SmartCard::ReaderStatus;
    ReaderStatus *const q;
public:
    explicit Private(ReaderStatus *qq)
        : ReaderStatusThread(qq),
          q(qq),
          watcher()
    {
        KDAB_SET_OBJECT_NAME(watcher);

        qRegisterMetaType<Card::Status>("Kleo::SmartCard::Card::Status");
        qRegisterMetaType<GpgME::Error>("GpgME::Error");

        watcher.whitelistFiles(QStringList(QStringLiteral("reader_*.status")));
        watcher.addPath(Kleo::gnupgHomeDirectory());
        watcher.setDelay(100);

        connect(this, &::ReaderStatusThread::cardChanged,
                q, &ReaderStatus::cardChanged);
        connect(this, &::ReaderStatusThread::anyCardHasNullPinChanged,
                q, &ReaderStatus::anyCardHasNullPinChanged);
        connect(this, &::ReaderStatusThread::anyCardCanLearnKeysChanged,
                q, &ReaderStatus::anyCardCanLearnKeysChanged);

        connect(&watcher, &FileSystemWatcher::triggered, this, &::ReaderStatusThread::ping);

    }
    ~Private()
    {
        stop();
        if (!wait(100)) {
            terminate();
            wait();
        }
    }

private:
    bool anyCardHasNullPinImpl() const
    {
        const auto cis = cardInfos();
        return std::any_of(cis.cbegin(), cis.cend(),
                           [](const std::shared_ptr<Card> &ci) { return ci->hasNullPin(); });
    }

    bool anyCardCanLearnKeysImpl() const
    {
        const auto cis = cardInfos();
        return std::any_of(cis.cbegin(), cis.cend(),
                           [](const std::shared_ptr<Card> &ci) { return ci->canLearnKeys(); });
    }

private:
    FileSystemWatcher watcher;
};

ReaderStatus::ReaderStatus(QObject *parent)
    : QObject(parent), d(new Private(this))
{
    self = this;
}

ReaderStatus::~ReaderStatus()
{
    self = nullptr;
}

// slot
void ReaderStatus::startMonitoring()
{
    d->start();
}

// static
ReaderStatus *ReaderStatus::mutableInstance()
{
    return self;
}

// static
const ReaderStatus *ReaderStatus::instance()
{
    return self;
}

Card::Status ReaderStatus::cardStatus(unsigned int slot) const
{
    return d->cardStatus(slot);
}

bool ReaderStatus::anyCardHasNullPin() const
{
    return d->anyCardHasNullPinImpl();
}

bool ReaderStatus::anyCardCanLearnKeys() const
{
    return d->anyCardCanLearnKeysImpl();
}

std::vector<Card::PinState> ReaderStatus::pinStates(unsigned int slot) const
{
    const auto ci = d->cardInfos();
    if (slot < ci.size()) {
        return ci[slot]->pinStates();
    } else {
        return std::vector<Card::PinState>();
    }
}

void ReaderStatus::startSimpleTransaction(const QByteArray &command, QObject *receiver, const char *slot)
{
    const Transaction t = { command, receiver, slot };
    d->addTransaction(t);
}

void ReaderStatus::updateStatus()
{
    d->ping();
}

std::vector <std::shared_ptr<Card> > ReaderStatus::getCards() const
{
    return d->cardInfos();
}

#include "readerstatus.moc"
