/* -*- mode: c++; c-basic-offset:4 -*-
    smartcard/readerstatus.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "readerstatus.h"

#include "keypairinfo.h"

#include <Libkleo/GnuPG>

#include <Libkleo/FileSystemWatcher>
#include <Libkleo/Stl_Util>

#include <gpgme++/context.h>
#include <gpgme++/defaultassuantransaction.h>
#include <gpgme++/key.h>

#include <gpg-error.h>

#include "kleopatra_debug.h"
#include "openpgpcard.h"
#include "netkeycard.h"
#include "pivcard.h"

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

#define xtoi_1(p)   (*(p) <= '9'? (*(p)- '0'): \
                     *(p) <= 'F'? (*(p)-'A'+10):(*(p)-'a'+10))
#define xtoi_2(p)   ((xtoi_1(p) * 16) + xtoi_1((p)+1))

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

Q_DECLARE_METATYPE(GpgME::Error)

namespace
{
static QDebug operator<<(QDebug s, const std::string &string)
{
    return s << QString::fromStdString(string);
}

static QDebug operator<<(QDebug s, const GpgME::Error &err)
{
    const bool oldSetting = s.autoInsertSpaces();
    s.nospace() << err.asString() << " (code: " << err.code() << ", source: " << err.source() << ")";
    s.setAutoInsertSpaces(oldSetting);
    return s.maybeSpace();
}

static QDebug operator<<(QDebug s, const std::vector< std::pair<std::string, std::string> > &v)
{
    typedef std::pair<std::string, std::string> pair;
    s << '(';
    for (const pair &p : v) {
        s << "status(" << QString::fromStdString(p.first) << ") =" << QString::fromStdString(p.second) << '\n';
    }
    return s << ')';
}

struct CardApp {
    std::string serialNumber;
    std::string appName;
};

static void logUnexpectedStatusLine(const std::pair<std::string, std::string> &line,
                                    const std::string &prefix = std::string(),
                                    const std::string &command = std::string())
{
    qCWarning(KLEOPATRA_LOG) << (!prefix.empty() ? QString::fromStdString(prefix + ": ") : QString())
            << "Unexpected status line"
            << (!command.empty() ? QString::fromStdString(" on " + command + ":") : QLatin1String(":"))
            << QString::fromStdString(line.first)
            << QString::fromStdString(line.second);
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

template<typename T>
static std::unique_ptr<T> gpgagent_transact(std::shared_ptr<Context> &gpgAgent, const char *command, std::unique_ptr<T> transaction, Error &err)
{
    qCDebug(KLEOPATRA_LOG) << "gpgagent_transact(" << command << ")";
    err = gpgAgent->assuanTransact(command, std::move(transaction));
    if (err.code()) {
        qCDebug(KLEOPATRA_LOG) << "gpgagent_transact(" << command << "): Error:" << err;
        if (err.code() >= GPG_ERR_ASS_GENERAL && err.code() <= GPG_ERR_ASS_UNKNOWN_INQUIRE) {
            qCDebug(KLEOPATRA_LOG) << "Assuan problem, killing context";
            gpgAgent.reset();
        }
        return std::unique_ptr<T>();
    }
    std::unique_ptr<AssuanTransaction> t = gpgAgent->takeLastAssuanTransaction();
    return std::unique_ptr<T>(dynamic_cast<T*>(t.release()));
}

static std::unique_ptr<DefaultAssuanTransaction> gpgagent_default_transact(std::shared_ptr<Context> &gpgAgent, const char *command, Error &err)
{
    return gpgagent_transact(gpgAgent, command, std::unique_ptr<DefaultAssuanTransaction>(new DefaultAssuanTransaction), err);
}

static const std::string gpgagent_data(std::shared_ptr<Context> gpgAgent, const char *command, Error &err)
{
    const std::unique_ptr<DefaultAssuanTransaction> t = gpgagent_default_transact(gpgAgent, command, err);
    if (t.get()) {
        qCDebug(KLEOPATRA_LOG) << "gpgagent_data(" << command << "): got" << QString::fromStdString(t->data());
        return t->data();
    } else {
        qCDebug(KLEOPATRA_LOG) << "gpgagent_data(" << command << "): t == NULL";
        return std::string();
    }
}

static const std::vector< std::pair<std::string, std::string> > gpgagent_statuslines(std::shared_ptr<Context> gpgAgent, const char *what, Error &err)
{
    const std::unique_ptr<DefaultAssuanTransaction> t = gpgagent_default_transact(gpgAgent, what, err);
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

static std::vector<CardApp> getCardsAndApps(std::shared_ptr<Context> &gpgAgent, Error &err)
{
    std::vector<CardApp> result;
    const std::string command = "SCD GETINFO all_active_apps";
    const auto statusLines = gpgagent_statuslines(gpgAgent, command.c_str(), err);
    if (err) {
        return result;
    }
    for (const auto &statusLine: statusLines) {
        if (statusLine.first == "SERIALNO") {
            const auto serialNumberAndApps = QByteArray::fromStdString(statusLine.second).split(' ');
            if (serialNumberAndApps.size() >= 2) {
                const auto serialNumber = serialNumberAndApps[0];
                auto apps = serialNumberAndApps.mid(1);
                // sort the apps to get a stable order independently of the currently selected application
                std::sort(apps.begin(), apps.end());
                for (const auto &app: apps) {
                    qCDebug(KLEOPATRA_LOG) << "getCardsAndApps(): Found card" << serialNumber << "with app" << app;
                    result.push_back({ serialNumber.toStdString(), app.toStdString() });
                }
            } else {
                logUnexpectedStatusLine(statusLine, "getCardsAndApps()", command);
            }
        } else {
            logUnexpectedStatusLine(statusLine, "getCardsAndApps()", command);
        }
    }
    return result;
}

static std::string switchCard(std::shared_ptr<Context> &gpgAgent, const std::string &serialNumber, Error &err)
{
    const std::string command = "SCD SWITCHCARD " + serialNumber;
    const auto statusLines = gpgagent_statuslines(gpgAgent, command.c_str(), err);
    if (err) {
        return std::string();
    }
    if (statusLines.size() == 1 && statusLines[0].first == "SERIALNO" && statusLines[0].second == serialNumber) {
        return serialNumber;
    }
    qCWarning(KLEOPATRA_LOG) << "switchCard():" << command << "returned" << statusLines
            << "(expected:" << "SERIALNO " + serialNumber << ")";
    return std::string();
}

static std::string switchApp(std::shared_ptr<Context> &gpgAgent, const std::string &serialNumber,
                             const std::string &appName, Error &err)
{
    const std::string command = "SCD SWITCHAPP " + appName;
    const auto statusLines = gpgagent_statuslines(gpgAgent, command.c_str(), err);
    if (err) {
        return std::string();
    }
    if (statusLines.size() == 1 && statusLines[0].first == "SERIALNO" &&
        statusLines[0].second.find(serialNumber + ' ' + appName) == 0) {
        return appName;
    }
    qCWarning(KLEOPATRA_LOG) << "switchApp():" << command << "returned" << statusLines
            << "(expected:" << "SERIALNO " + serialNumber + ' ' + appName + "..." << ")";
    return std::string();
}

static const char * get_openpgp_card_manufacturer_from_serial_number(const std::string &serialno)
{
    qCDebug(KLEOPATRA_LOG) << "get_openpgp_card_manufacturer_from_serial_number(" << serialno.c_str() << ")";

    const bool isProperOpenPGPCardSerialNumber =
        serialno.size() == 32 && serialno.substr(0, 12) == "D27600012401";
    if (isProperOpenPGPCardSerialNumber) {
        const char *sn = serialno.c_str();
        const int manufacturerId = xtoi_2(sn + 16)*256 + xtoi_2(sn + 18);
        switch (manufacturerId) {
            case 0x0001: return "PPC Card Systems";
            case 0x0002: return "Prism";
            case 0x0003: return "OpenFortress";
            case 0x0004: return "Wewid";
            case 0x0005: return "ZeitControl";
            case 0x0006: return "Yubico";
            case 0x0007: return "OpenKMS";
            case 0x0008: return "LogoEmail";

            case 0x002A: return "Magrathea";

            case 0x1337: return "Warsaw Hackerspace";

            case 0xF517: return "FSIJ";

            /* 0x0000 and 0xFFFF are defined as test cards per spec,
               0xFF00 to 0xFFFE are assigned for use with randomly created
               serial numbers.  */
            case 0x0000:
            case 0xffff: return "test card";
            default: return (manufacturerId & 0xff00) == 0xff00 ? "unmanaged S/N range" : "unknown";
        }
    } else {
        return "unknown";
    }
}

static const std::string get_manufacturer(std::shared_ptr<Context> &gpgAgent, Error &err)
{
    // The result of SCD GETATTR MANUFACTURER is the manufacturer ID as unsigned number
    // optionally followed by the name of the manufacturer, e.g.
    // 6 Yubico
    // 65534 unmanaged S/N range
    const auto manufacturerIdAndName = scd_getattr_status(gpgAgent, "MANUFACTURER", err);
    if (err.code()) {
        if (err.code() == GPG_ERR_INV_NAME) {
            qCDebug(KLEOPATRA_LOG) << "get_manufacturer(): Querying for attribute MANUFACTURER not yet supported; needs GnuPG 2.2.21+";
        } else {
            qCWarning(KLEOPATRA_LOG) << "Running SCD GETATTR MANUFACTURER failed:" << err;
        }
        return std::string();
    }
    const auto startOfManufacturerName = manufacturerIdAndName.find(' ');
    if (startOfManufacturerName == std::string::npos) {
        // only ID without manufacturer name
        return "unknown";
    }
    const auto manufacturerName = manufacturerIdAndName.substr(startOfManufacturerName + 1);
    return manufacturerName;
}

static const std::string getDisplaySerialNumber(std::shared_ptr<Context> &gpgAgent, Error &err)
{
    const auto displaySerialnumber = scd_getattr_status(gpgAgent, "$DISPSERIALNO", err);
    if (err && err.code() != GPG_ERR_INV_NAME) {
        qCWarning(KLEOPATRA_LOG) << "Running SCD GETATTR $DISPSERIALNO failed:" << err;
    }
    return displaySerialnumber;
}

static bool setDisplaySerialNumber(Card *card, std::shared_ptr<Context> &gpgAgent)
{
    Error err;
    const std::string displaySerialnumber = getDisplaySerialNumber(gpgAgent, err);
    card->setDisplaySerialNumber(err ? QString::fromStdString(card->serialNumber()) : QString::fromStdString(displaySerialnumber));
    return !err;
}

static void handle_openpgp_card(std::shared_ptr<Card> &ci, std::shared_ptr<Context> &gpg_agent)
{
    Error err;
    auto pgpCard = new OpenPGPCard(*ci);

    pgpCard->setManufacturer(get_manufacturer(gpg_agent, err));
    if (err.code()) {
        // fallback, e.g. if gpg does not yet support querying for the MANUFACTURER attribute
        pgpCard->setManufacturer(get_openpgp_card_manufacturer_from_serial_number(ci->serialNumber()));
    }

    const auto info = gpgagent_statuslines(gpg_agent, "SCD LEARN --force", err);
    if (err.code()) {
        ci->setStatus(Card::CardError);
        return;
    }
    pgpCard->setCardInfo(info);

    if (!setDisplaySerialNumber(pgpCard, gpg_agent)) {
        pgpCard->setDisplaySerialNumber(QString::fromStdString(pgpCard->serialNumber()).mid(16, 12));
    }

    ci.reset(pgpCard);
}

static void readKeyPairInfoFromPIVCard(const std::string &keyRef, PIVCard *pivCard, const std::shared_ptr<Context> &gpg_agent)
{
    Error err;
    const std::string command = std::string("SCD READKEY --info-only -- ") + keyRef;
    const auto keyPairInfoLines = gpgagent_statuslines(gpg_agent, command.c_str(), err);
    if (err) {
        qCWarning(KLEOPATRA_LOG) << "Running" << command << "failed:" << err;
        return;
    }
    for (const auto &pair: keyPairInfoLines) {
        if (pair.first == "KEYPAIRINFO") {
            const KeyPairInfo info = KeyPairInfo::fromStatusLine(pair.second);
            if (info.grip.empty()) {
                qCWarning(KLEOPATRA_LOG) << "Invalid KEYPAIRINFO status line"
                        << QString::fromStdString(pair.second);
                continue;
            }
            pivCard->setKeyAlgorithm(keyRef, info.algorithm);
        } else {
            logUnexpectedStatusLine(pair, "readKeyPairInfoFromPIVCard()", command);
        }
    }
}

static void readCertificateFromPIVCard(const std::string &keyRef, PIVCard *pivCard, const std::shared_ptr<Context> &gpg_agent)
{
    Error err;
    const std::string command = std::string("SCD READCERT ") + keyRef;
    const std::string certificateData = gpgagent_data(gpg_agent, command.c_str(), err);
    if (err && err.code() != GPG_ERR_NOT_FOUND) {
        qCWarning(KLEOPATRA_LOG) << "Running" << command << "failed:" << err;
        return;
    }
    if (certificateData.empty()) {
        qCDebug(KLEOPATRA_LOG) << "readCertificateFromPIVCard(" << QString::fromStdString(keyRef) << "): No certificate stored on card";
        return;
    }
    qCDebug(KLEOPATRA_LOG) << "readCertificateFromPIVCard(" << QString::fromStdString(keyRef) << "): Found certificate stored on card";
    pivCard->setCertificateData(keyRef, certificateData);
}

static void handle_piv_card(std::shared_ptr<Card> &ci, std::shared_ptr<Context> &gpg_agent)
{
    Error err;
    auto pivCard = new PIVCard(*ci);

    const auto info = gpgagent_statuslines(gpg_agent, "SCD LEARN --force", err);
    if (err) {
        ci->setStatus(Card::CardError);
        return;
    }
    pivCard->setCardInfo(info);

    (void) setDisplaySerialNumber(pivCard, gpg_agent);

    for (const std::string &keyRef : PIVCard::supportedKeys()) {
        if (!pivCard->keyGrip(keyRef).empty()) {
            readKeyPairInfoFromPIVCard(keyRef, pivCard, gpg_agent);
            readCertificateFromPIVCard(keyRef, pivCard, gpg_agent);
        }
    }

    ci.reset(pivCard);
}

static void handle_netkey_card(std::shared_ptr<Card> &ci, std::shared_ptr<Context> &gpg_agent)
{
    Error err;
    auto nkCard = new NetKeyCard(*ci);
    ci.reset(nkCard);

    ci->setAppVersion(parse_app_version(scd_getattr_status(gpg_agent, "NKS-VERSION", err)));

    if (err.code()) {
        qCWarning(KLEOPATRA_LOG) << "Running SCD GETATTR NKS-VERSION failed:" << err;
        ci->setErrorMsg(QStringLiteral ("NKS-VERSION failed: ") + QString::fromUtf8(err.asString()));
        return;
    }

    if (ci->appVersion() != 3) {
        qCDebug(KLEOPATRA_LOG) << "not a NetKey v3 card, giving up. Version:" << ci->appVersion();
        ci->setErrorMsg(QStringLiteral("NetKey v%1 cards are not supported.").arg(ci->appVersion()));
        return;
    }

    (void) setDisplaySerialNumber(nkCard, gpg_agent);

    // the following only works for NKS v3...
    const auto chvStatus = QString::fromStdString(
            scd_getattr_status(gpg_agent, "CHV-STATUS", err)).split(QLatin1Char(' '));
    if (err.code()) {
        qCDebug(KLEOPATRA_LOG) << "Running SCD GETATTR CHV-STATUS failed:" << err;
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
    const std::unique_ptr<DefaultAssuanTransaction> result = gpgagent_default_transact(gpg_agent, "SCD LEARN --keypairinfo", err);
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

static std::shared_ptr<Card> get_card_status(const std::string &serialNumber, const std::string &appName, std::shared_ptr<Context> &gpg_agent)
{
    qCDebug(KLEOPATRA_LOG) << "get_card_status(" << serialNumber << ',' << appName << ',' << gpg_agent.get() << ')';
    auto ci = std::shared_ptr<Card>(new Card());

    // select card
    {
        Error err;
        const auto result = switchCard(gpg_agent, serialNumber, err);
        if (err) {
            if (err.code() == GPG_ERR_CARD_NOT_PRESENT || err.code() == GPG_ERR_CARD_REMOVED) {
                ci->setStatus(Card::NoCard);
            } else {
                ci->setStatus(Card::CardError);
            }
            return ci;
        }
        if (result.empty()) {
            qCWarning(KLEOPATRA_LOG) << "get_card_status: switching card failed";
            ci->setStatus(Card::CardError);
            return ci;
        }
        ci->setStatus(Card::CardPresent);
    }

    // select app
    {
        Error err;
        const auto result = switchApp(gpg_agent, serialNumber, appName, err);
        if (err) {
            if (err.code() == GPG_ERR_CARD_NOT_PRESENT || err.code() == GPG_ERR_CARD_REMOVED) {
                ci->setStatus(Card::NoCard);
            } else {
                ci->setStatus(Card::CardError);
            }
            return ci;
        }
        if (result.empty()) {
            qCWarning(KLEOPATRA_LOG) << "get_card_status: switching app failed";
            ci->setStatus(Card::CardError);
            return ci;
        }
    }

    ci->setSerialNumber(serialNumber);

    // Handle different card types
    if (appName == NetKeyCard::AppName) {
        qCDebug(KLEOPATRA_LOG) << "get_card_status: found Netkey card" << ci->serialNumber().c_str() << "end";
        handle_netkey_card(ci, gpg_agent);
        return ci;
    } else if (appName == OpenPGPCard::AppName) {
        qCDebug(KLEOPATRA_LOG) << "get_card_status: found OpenPGP card" << ci->serialNumber().c_str() << "end";
        handle_openpgp_card(ci, gpg_agent);
        return ci;
    } else if (appName == PIVCard::AppName) {
        qCDebug(KLEOPATRA_LOG) << "get_card_status: found PIV card" << ci->serialNumber().c_str() << "end";
        handle_piv_card(ci, gpg_agent);
        return ci;
    } else {
        qCDebug(KLEOPATRA_LOG) << "get_card_status: unhandled application:" << appName;
        return ci;
    }

    return ci;
}

static bool isCardNotPresentError(const GpgME::Error &err)
{
    // see fixup_scd_errors() in gpg-card.c
    return err && ((err.code() == GPG_ERR_CARD_NOT_PRESENT) ||
                   ((err.code() == GPG_ERR_ENODEV || err.code() == GPG_ERR_CARD_REMOVED) &&
                    (err.sourceID() == GPG_ERR_SOURCE_SCD)));
}

static std::vector<std::shared_ptr<Card> > update_cardinfo(std::shared_ptr<Context> &gpgAgent)
{
    qCDebug(KLEOPATRA_LOG) << "update_cardinfo()";

    // ensure that a card is present and that all cards are properly set up
    {
        Error err;
        const char *command = "SCD SERIALNO --all";
        const std::string serialno = gpgagent_status(gpgAgent, command, err);
        if (err) {
            if (isCardNotPresentError(err)) {
                qCDebug(KLEOPATRA_LOG) << "update_cardinfo: No card present";
                return std::vector<std::shared_ptr<Card> >();
            } else {
                qCWarning(KLEOPATRA_LOG) << "Running" << command << "failed:" << err;
                auto ci = std::shared_ptr<Card>(new Card());
                ci->setStatus(Card::CardError);
                return std::vector<std::shared_ptr<Card> >(1, ci);
            }
        }
    }

    Error err;
    const std::vector<CardApp> cardApps = getCardsAndApps(gpgAgent, err);
    if (err) {
        if (isCardNotPresentError(err)) {
            qCDebug(KLEOPATRA_LOG) << "update_cardinfo: No card present";
            return std::vector<std::shared_ptr<Card> >();
        } else {
            qCWarning(KLEOPATRA_LOG) << "Getting active apps on all inserted cards failed:" << err;
            auto ci = std::shared_ptr<Card>(new Card());
            ci->setStatus(Card::CardError);
            return std::vector<std::shared_ptr<Card> >(1, ci);
        }
    }

    std::vector<std::shared_ptr<Card> > cards;
    for (const auto &cardApp: cardApps) {
        const auto card = get_card_status(cardApp.serialNumber, cardApp.appName, gpgAgent);
        cards.push_back(card);
    }
    return cards;
}
} // namespace

struct Transaction {
    CardApp cardApp;
    QByteArray command;
    QPointer<QObject> receiver;
    const char *slot;
    AssuanTransaction* assuanTransaction;
};

static const Transaction updateTransaction = { { "__all__", "__all__" }, "__update__", nullptr, nullptr, nullptr };
static const Transaction quitTransaction   = { { "__all__", "__all__" }, "__quit__",   nullptr, nullptr, nullptr };

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
    void firstCardWithNullPinChanged(const std::string &serialNumber);
    void anyCardCanLearnKeysChanged(bool);
    void cardAdded(const std::string &serialNumber, const std::string &appName);
    void cardChanged(const std::string &serialNumber, const std::string &appName);
    void cardRemoved(const std::string &serialNumber, const std::string &appName);
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

            CardApp cardApp;
            QByteArray command;
            bool nullSlot = false;
            AssuanTransaction* assuanTransaction = nullptr;
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
                cardApp = item.front().cardApp;
                command = item.front().command;
                nullSlot = !item.front().slot;
                // we take ownership of the assuan transaction
                std::swap(assuanTransaction, item.front().assuanTransaction);
                oldCards = m_cardInfos;
            }

            qCDebug(KLEOPATRA_LOG) << "ReaderStatusThread[2nd]: new iteration command=" << command << " ; nullSlot=" << nullSlot;
            // now, let's see what we got:
            if (nullSlot && command == quitTransaction.command) {
                return;    // quit
            }

            if ((nullSlot && command == updateTransaction.command)) {

                std::vector<std::shared_ptr<Card> > newCards = update_cardinfo(gpgAgent);

                KDAB_SYNCHRONIZED(m_mutex)
                m_cardInfos = newCards;

                bool anyLC = false;
                std::string firstCardWithNullPin;
                bool anyError = false;
                for (const auto &newCard: newCards) {
                    const auto serialNumber = newCard->serialNumber();
                    const auto appName = newCard->appName();
                    const auto matchingOldCard = std::find_if(oldCards.cbegin(), oldCards.cend(),
                        [serialNumber, appName] (const std::shared_ptr<Card> &card) {
                            return card->serialNumber() == serialNumber && card->appName() == appName;
                        });
                    if (matchingOldCard == oldCards.cend()) {
                        qCDebug(KLEOPATRA_LOG) << "ReaderStatusThread: Card" << serialNumber << "with app" << appName << "was added";
                        Q_EMIT cardAdded(serialNumber, appName);
                    } else {
                        if (*newCard != **matchingOldCard) {
                            qCDebug(KLEOPATRA_LOG) << "ReaderStatusThread: Card" << serialNumber << "with app" << appName << "changed";
                            Q_EMIT cardChanged(serialNumber, appName);
                        }
                        oldCards.erase(matchingOldCard);
                    }
                    if (newCard->canLearnKeys()) {
                        anyLC = true;
                    }
                    if (newCard->hasNullPin() && firstCardWithNullPin.empty()) {
                        firstCardWithNullPin = newCard->serialNumber();
                    }
                    if (newCard->status() == Card::CardError) {
                        anyError = true;
                    }
                }
                for (const auto &oldCard: oldCards) {
                    qCDebug(KLEOPATRA_LOG) << "ReaderStatusThread: Card" << oldCard->serialNumber() << "with app" << oldCard->appName() << "was removed";
                    Q_EMIT cardRemoved(oldCard->serialNumber(), oldCard->appName());
                }

                Q_EMIT firstCardWithNullPinChanged(firstCardWithNullPin);
                Q_EMIT anyCardCanLearnKeysChanged(anyLC);

                if (anyError) {
                    gpgAgent.reset();
                }
            } else {
                GpgME::Error err;
                switchCard(gpgAgent, cardApp.serialNumber, err);
                if (!err) {
                    switchApp(gpgAgent, cardApp.serialNumber, cardApp.appName, err);
                }
                if (!err) {
                    if (assuanTransaction) {
                        (void)gpgagent_transact(gpgAgent, command.constData(), std::unique_ptr<AssuanTransaction>(assuanTransaction), err);
                    } else {
                        (void)gpgagent_default_transact(gpgAgent, command.constData(), err);
                    }
                }

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

        connect(this, &::ReaderStatusThread::cardAdded,
                q, &ReaderStatus::cardAdded);
        connect(this, &::ReaderStatusThread::cardChanged,
                q, &ReaderStatus::cardChanged);
        connect(this, &::ReaderStatusThread::cardRemoved,
                q, &ReaderStatus::cardRemoved);
        connect(this, &::ReaderStatusThread::firstCardWithNullPinChanged,
                q, &ReaderStatus::firstCardWithNullPinChanged);
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
    std::string firstCardWithNullPinImpl() const
    {
        const auto cis = cardInfos();
        const auto firstWithNullPin = std::find_if(cis.cbegin(), cis.cend(),
                                                   [](const std::shared_ptr<Card> &ci) { return ci->hasNullPin(); });
        return firstWithNullPin != cis.cend() ? (*firstWithNullPin)->serialNumber() : std::string();
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

    qRegisterMetaType<std::string>("std::string");
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

std::string ReaderStatus::firstCardWithNullPin() const
{
    return d->firstCardWithNullPinImpl();
}

bool ReaderStatus::anyCardCanLearnKeys() const
{
    return d->anyCardCanLearnKeysImpl();
}

void ReaderStatus::startSimpleTransaction(const std::shared_ptr<Card> &card, const QByteArray &command, QObject *receiver, const char *slot)
{
    const CardApp cardApp = { card->serialNumber(), card->appName() };
    const Transaction t = { cardApp, command, receiver, slot, nullptr };
    d->addTransaction(t);
}

void ReaderStatus::startTransaction(const std::shared_ptr<Card> &card, const QByteArray &command, QObject *receiver, const char *slot,
                                    std::unique_ptr<AssuanTransaction> transaction)
{
    const CardApp cardApp = { card->serialNumber(), card->appName() };
    const Transaction t = { cardApp, command, receiver, slot, transaction.release() };
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

std::shared_ptr<Card> ReaderStatus::getCard(const std::string &serialNumber, const std::string &appName) const
{
    for (const auto &card: d->cardInfos()) {
        if (card->serialNumber() == serialNumber && card->appName() == appName) {
            qCDebug(KLEOPATRA_LOG) << "ReaderStatus::getCard() - Found card with serial number" << serialNumber << "and app" << appName;
            return card;
        }
    }
    qCWarning(KLEOPATRA_LOG) << "ReaderStatus::getCard() - Did not find card with serial number" << serialNumber << "and app" << appName;
    return std::shared_ptr<Card>();
}

#include "readerstatus.moc"
