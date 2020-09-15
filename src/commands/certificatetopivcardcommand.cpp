/* commands/certificatetopivcardcommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "certificatetopivcardcommand.h"

#include "command_p.h"

#include "smartcard/readerstatus.h"
#include "smartcard/pivcard.h"

#include "commands/authenticatepivcardapplicationcommand.h"

#include "utils/writecertassuantransaction.h"

#include <Libkleo/KeyCache>

#include <KLocalizedString>

#include <qgpgme/dataprovider.h>

#include <gpgme++/context.h>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::SmartCard;
using namespace GpgME;

class CertificateToPIVCardCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::CertificateToPIVCardCommand;
    CertificateToPIVCardCommand *q_func() const
    {
        return static_cast<CertificateToPIVCardCommand *>(q);
    }
public:
    explicit Private(CertificateToPIVCardCommand *qq, const GpgME::Subkey &key, const std::string &serialno);
    explicit Private(CertificateToPIVCardCommand *qq, const std::string &cardSlot, const std::string &serialno);
    ~Private();

private:
    void start();
    void startCertificateToPIVCard();

    void authenticate();
    void authenticationFinished();
    void authenticationCanceled();

private:
    std::string mSerial;
    GpgME::Subkey mSubkey;
    std::string cardSlot;
    bool overwriteExistingAlreadyApproved = false;
    bool hasBeenCanceled = false;
};

CertificateToPIVCardCommand::Private *CertificateToPIVCardCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const CertificateToPIVCardCommand::Private *CertificateToPIVCardCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define q q_func()
#define d d_func()


CertificateToPIVCardCommand::Private::Private(CertificateToPIVCardCommand *qq,
                                   const GpgME::Subkey &key,
                                   const std::string &serialno)
    : Command::Private(qq, nullptr),
      mSerial(serialno),
      mSubkey(key)
{
}

CertificateToPIVCardCommand::Private::Private(CertificateToPIVCardCommand *qq, const std::string &cardSlot_, const std::string &serialno)
    : Command::Private(qq, nullptr)
    , mSerial(serialno)
    , cardSlot(cardSlot_)
{
}

CertificateToPIVCardCommand::Private::~Private()
{
}

namespace {
static GpgME::Subkey getSubkeyToTransferToPIVCard(const std::string &cardSlot, const std::shared_ptr<PIVCard> &card)
{
    if (!cardSlot.empty()) {
        if (cardSlot == PIVCard::digitalSignatureKeyRef()) {
            // get signing certificate matching the key grip
            const std::string cardKeygrip = card->keyGrip(cardSlot);
            const auto subkey = KeyCache::instance()->findSubkeyByKeyGrip(cardKeygrip);
            if (subkey.canSign() && subkey.parent().protocol() == GpgME::CMS) {
                return subkey;
            }
        }
        if (cardSlot == PIVCard::keyManagementKeyRef()) {
            // get encryption certificate with secret subkey
        }
        return GpgME::Subkey();
    }

    return GpgME::Subkey();
}
}

void CertificateToPIVCardCommand::Private::start()
{
    qCDebug(KLEOPATRA_LOG) << "CertificateToPIVCardCommand::Private::start()";

    const auto pivCard = SmartCard::ReaderStatus::instance()->getCard<PIVCard>(mSerial);
    if (!pivCard) {
        error(i18n("Failed to find the PIV card with the serial number: %1", QString::fromStdString(mSerial)));
        finished();
        return;
    }

    mSubkey = getSubkeyToTransferToPIVCard(cardSlot, pivCard);
    if (mSubkey.isNull()) {
        error(i18n("Sorry! No suitable certificate to write to this card slot was found."));
        finished();
        return;
    }

    startCertificateToPIVCard();
}

void CertificateToPIVCardCommand::Private::startCertificateToPIVCard()
{
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::Private::startCertificateToPIVCard()";

    auto ctx = Context::createForProtocol(GpgME::CMS);
    QGpgME::QByteArrayDataProvider dp;
    Data data(&dp);
    const Error err = ctx->exportPublicKeys(mSubkey.parent().primaryFingerprint(), data);
    if (err) {
        error(i18nc("@info", "Exporting the certificate failed: %1", QString::fromUtf8(err.asString())),
              i18nc("@title", "Error"));
        finished();
        return;
    }
    const QByteArray certificateData = dp.data();

    const QString cmd = QStringLiteral("SCD WRITECERT %1")
        .arg(QString::fromStdString(cardSlot));
    auto transaction = std::unique_ptr<AssuanTransaction>(new WriteCertAssuanTransaction(certificateData));
    ReaderStatus::mutableInstance()->startTransaction(cmd.toUtf8(), q_func(), "certificateToPIVCardDone", std::move(transaction));
}

void CertificateToPIVCardCommand::Private::authenticate()
{
    qCDebug(KLEOPATRA_LOG) << "CertificateToPIVCardCommand::authenticate()";

    auto cmd = new AuthenticatePIVCardApplicationCommand(mSerial, parentWidgetOrView());
    connect(cmd, &AuthenticatePIVCardApplicationCommand::finished,
            q, [this]() { authenticationFinished(); });
    connect(cmd, &AuthenticatePIVCardApplicationCommand::canceled,
            q, [this]() { authenticationCanceled(); });
    cmd->start();
}

void CertificateToPIVCardCommand::Private::authenticationFinished()
{
    qCDebug(KLEOPATRA_LOG) << "CertificateToPIVCardCommand::authenticationFinished()";
    if (!hasBeenCanceled) {
        startCertificateToPIVCard();
    }
}

void CertificateToPIVCardCommand::Private::authenticationCanceled()
{
    qCDebug(KLEOPATRA_LOG) << "CertificateToPIVCardCommand::authenticationCanceled()";
    hasBeenCanceled = true;
    canceled();
}

CertificateToPIVCardCommand::CertificateToPIVCardCommand(const std::string& cardSlot, const std::string &serialno)
    : Command(new Private(this, cardSlot, serialno))
{
}

CertificateToPIVCardCommand::~CertificateToPIVCardCommand()
{
    qCDebug(KLEOPATRA_LOG) << "CertificateToPIVCardCommand::~CertificateToPIVCardCommand()";
}

void CertificateToPIVCardCommand::certificateToPIVCardDone(const Error &err)
{
    qCDebug(KLEOPATRA_LOG) << "CertificateToPIVCardCommand::certificateToPIVCardDone():"
                           << err.asString() << "(" << err.code() << ")";
    if (err) {
        // gpgme 1.13 reports "BAD PIN" instead of "NO AUTH"
        if (err.code() == GPG_ERR_NO_AUTH || err.code() == GPG_ERR_BAD_PIN) {
            d->authenticate();
            return;
        }

        d->error(i18nc("@info",
                       "Writing the certificate to the card failed: %1", QString::fromUtf8(err.asString())),
                        i18nc("@title", "Error"));
    } else if (!err.isCanceled()) {
        KMessageBox::information(d->parentWidgetOrView(),
                                 i18n("Successfully copied the certificate to the card."),
                                 i18nc("@title", "Success"));
        ReaderStatus::mutableInstance()->updateStatus();
    }

    d->finished();
}

void CertificateToPIVCardCommand::doStart()
{
    qCDebug(KLEOPATRA_LOG) << "CertificateToPIVCardCommand::doStart()";

    d->start();
}

void CertificateToPIVCardCommand::doCancel()
{
}

#undef q_func
#undef d_func
