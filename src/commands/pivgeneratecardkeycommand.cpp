/*  commands/pivgeneratecardkeycommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pivgeneratecardkeycommand.h"

#include "cardcommand_p.h"

#include "smartcard/readerstatus.h"

#include <gpgme++/error.h>

#include <KLocalizedString>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::SmartCard;
using namespace GpgME;

class PIVGenerateCardKeyCommand::Private : public CardCommand::Private
{
    friend class ::Kleo::Commands::PIVGenerateCardKeyCommand;
    PIVGenerateCardKeyCommand *q_func() const
    {
        return static_cast<PIVGenerateCardKeyCommand *>(q);
    }
public:
    explicit Private(PIVGenerateCardKeyCommand *qq, QWidget *p);
    ~Private();

    void init();

private:
    void slotAuthenticateResult(const Error &err);
    void slotGenerateKeyResult(const Error &err);

private:
    void authenticate();
    void generateKey();

private:
    QByteArray keyref;
};

PIVGenerateCardKeyCommand::Private *PIVGenerateCardKeyCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const PIVGenerateCardKeyCommand::Private *PIVGenerateCardKeyCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

PIVGenerateCardKeyCommand::Private::Private(PIVGenerateCardKeyCommand *qq, QWidget *p)
    : CardCommand::Private(qq, p)
{
}

PIVGenerateCardKeyCommand::Private::~Private()
{
    qCDebug(KLEOPATRA_LOG) << "PIVGenerateCardKeyCommand::Private::~Private()";
}

PIVGenerateCardKeyCommand::PIVGenerateCardKeyCommand(QWidget *p)
    : CardCommand(new Private(this, p))
{
    d->init();
}

void PIVGenerateCardKeyCommand::Private::init()
{
}

PIVGenerateCardKeyCommand::~PIVGenerateCardKeyCommand()
{
    qCDebug(KLEOPATRA_LOG) << "PIVGenerateCardKeyCommand::~PIVGenerateCardKeyCommand()";
}

void PIVGenerateCardKeyCommand::setKeyRef(const QByteArray &keyref)
{
    d->keyref = keyref;
}

void PIVGenerateCardKeyCommand::doStart()
{
    qCDebug(KLEOPATRA_LOG) << "PIVGenerateCardKeyCommand::doStart()";

    d->generateKey();
}

void PIVGenerateCardKeyCommand::Private::authenticate()
{
    qCDebug(KLEOPATRA_LOG) << "PIVGenerateCardKeyCommand::authenticate()";

    const QByteArray defaultAuthenticationKey = QByteArray::fromHex("010203040506070801020304050607080102030405060708");
    const QByteArray plusPercentEncodedAuthenticationKey = defaultAuthenticationKey.toPercentEncoding().replace(' ', '+');
    const QByteArray command = QByteArray("SCD SETATTR AUTH-ADM-KEY ") + plusPercentEncodedAuthenticationKey;
    ReaderStatus::mutableInstance()->startSimpleTransaction(command, q, "slotAuthenticateResult");
}

void PIVGenerateCardKeyCommand::Private::slotAuthenticateResult(const Error &err)
{
    qCDebug(KLEOPATRA_LOG) << "PIVGenerateCardKeyCommand::slotAuthenticateResult():"
                           << err.asString() << "(" << err.code() << ")";
    if (err) {
        error(i18nc("@info", "Authenticating to the card failed: %1", QString::fromLatin1(err.asString())),
              i18nc("@title", "Error"));
        finished();
    } else if (err.isCanceled()) {
        finished();
    } else {
        generateKey();
    }
}

void PIVGenerateCardKeyCommand::Private::generateKey()
{
    qCDebug(KLEOPATRA_LOG) << "PIVGenerateCardKeyCommand::generateKey()";

    ReaderStatus::mutableInstance()->startSimpleTransaction("SCD GENKEY -- " + keyref, q, "slotGenerateKeyResult");
}

void PIVGenerateCardKeyCommand::Private::slotGenerateKeyResult(const GpgME::Error& err)
{
    qCDebug(KLEOPATRA_LOG) << "PIVGenerateCardKeyCommand::slotGenerateKeyResult():"
                           << err.asString() << "(" << err.code() << ")";
    if (err) {
        if (err.code() == GPG_ERR_NO_AUTH) {
            authenticate();
            return;
        }

        error(i18nc("@info", "Generating key failed: %1", QString::fromLatin1(err.asString())),
              i18nc("@title", "Error"));
    } else if (!err.isCanceled()) {
        information(i18nc("@info", "Key successfully generated."), i18nc("@title", "Success"));
        ReaderStatus::mutableInstance()->updateStatus();
    }
    finished();
}

#undef d
#undef q

#include "moc_pivgeneratecardkeycommand.cpp"
