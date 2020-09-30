/* commands/keytocardcommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "keytocardcommand.h"

#include "cardcommand_p.h"

#include "commands/authenticatepivcardapplicationcommand.h"

#include "dialogs/certificateselectiondialog.h"

#include "smartcard/openpgpcard.h"
#include "smartcard/pivcard.h"
#include "smartcard/readerstatus.h"

#include <Libkleo/Dn>
#include <Libkleo/Formatting>
#include <Libkleo/KeyCache>
#include <Libkleo/KeySelectionDialog>

#include <KLocalizedString>

#include <QDateTime>
#include <QInputDialog>
#include <QStringList>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;
using namespace Kleo::SmartCard;
using namespace GpgME;

class KeyToCardCommand::Private : public CardCommand::Private
{
    friend class ::Kleo::Commands::KeyToCardCommand;
    KeyToCardCommand *q_func() const
    {
        return static_cast<KeyToCardCommand *>(q);
    }
public:
    explicit Private(KeyToCardCommand *qq, const GpgME::Subkey &subkey, const std::string &serialno);
    explicit Private(KeyToCardCommand *qq, const std::string &slot, const std::string &serialno);
    ~Private();

private:
    void start();

    void startKeyToOpenPGPCard();

    Subkey getSubkeyToTransferToPIVCard(const std::string &cardSlot, const std::shared_ptr<PIVCard> &card);
    void startKeyToPIVCard();

    void authenticate();
    void authenticationFinished();
    void authenticationCanceled();

private:
    GpgME::Subkey subkey;
    std::string cardSlot;
    bool overwriteExistingAlreadyApproved = false;
    bool hasBeenCanceled = false;
};

KeyToCardCommand::Private *KeyToCardCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const KeyToCardCommand::Private *KeyToCardCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define q q_func()
#define d d_func()


KeyToCardCommand::Private::Private(KeyToCardCommand *qq,
                                   const GpgME::Subkey &subkey_,
                                   const std::string &serialno)
    : CardCommand::Private(qq, serialno, nullptr),
      subkey(subkey_)
{
}

KeyToCardCommand::Private::Private(KeyToCardCommand *qq, const std::string &slot, const std::string &serialno)
    : CardCommand::Private(qq, serialno, nullptr)
    , cardSlot(slot)
{
}

KeyToCardCommand::Private::~Private()
{
}

void KeyToCardCommand::Private::start()
{
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::Private::start()";

    const auto card = SmartCard::ReaderStatus::instance()->getCard<Card>(serialNumber());
    if (!card) {
        error(i18n("Failed to find the card with the serial number: %1", QString::fromStdString(serialNumber())));
        finished();
        return;
    }

    if (card->appName() == SmartCard::OpenPGPCard::AppName) {
        startKeyToOpenPGPCard();
    } else if (card->appName() == SmartCard::PIVCard::AppName) {
        startKeyToPIVCard();
    } else {
        error(i18n("Sorry! Transferring keys to this card is not supported."));
        finished();
        return;
    }
}

namespace {
static int getOpenPGPCardSlotForKey(const GpgME::Subkey &subKey, QWidget *parent)
{
    // Check if we need to ask the user for the slot
    if ((subKey.canSign() || subKey.canCertify()) && !subKey.canEncrypt() && !subKey.canAuthenticate()) {
        // Signing only
        return 1;
    }
    if (subKey.canEncrypt() && !(subKey.canSign() || subKey.canCertify()) && !subKey.canAuthenticate()) {
        // Encrypt only
        return 2;
    }
    if (subKey.canAuthenticate() && !(subKey.canSign() || subKey.canCertify()) && !subKey.canEncrypt()) {
        // Auth only
        return 3;
    }
    // Multiple uses, ask user.
    QStringList options;

    if (subKey.canSign() || subKey.canCertify()) {
        options << i18nc("Placeholder is the number of a slot on a smart card", "Signature (%1)", 1);
    }
    if (subKey.canEncrypt()) {
        options << i18nc("Placeholder is the number of a slot on a smart card", "Encryption (%1)", 2);
    }
    if (subKey.canAuthenticate()) {
        options << i18nc("Placeholder is the number of a slot on a smart card", "Authentication (%1)", 3);
    }

    bool ok;
    const QString choice = QInputDialog::getItem(parent, i18n("Select Card Slot"),
        i18n("Please select the card slot the key should be written to:"), options, /* current= */ 0, /* editable= */ false, &ok);
    const int slot = options.indexOf(choice) + 1;
    return ok ? slot : -1;
}
}

void KeyToCardCommand::Private::startKeyToOpenPGPCard() {
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::Private::startKeyToOpenPGPCard()";

    const auto pgpCard = SmartCard::ReaderStatus::instance()->getCard<OpenPGPCard>(serialNumber());
    if (!pgpCard) {
        error(i18n("Failed to find the OpenPGP card with the serial number: %1", QString::fromStdString(serialNumber())));
        finished();
        return;
    }

    if (subkey.isNull()) {
        finished();
        return;
    }
    if (subkey.parent().protocol() != GpgME::OpenPGP) {
        error(i18n("Sorry! This key cannot be transferred to an OpenPGP card."));
        finished();
        return;
    }

    const auto slot = getOpenPGPCardSlotForKey(subkey, parentWidgetOrView());
    if (slot < 1) {
        finished();
        return;
    }

    // Check if we need to do the overwrite warning.
    std::string existingKey;
    QString encKeyWarning;
    if (slot == 1) {
        existingKey = pgpCard->sigFpr();
    } else if (slot == 2) {
        existingKey = pgpCard->encFpr();
        encKeyWarning = i18n("It will no longer be possible to decrypt past communication "
                                "encrypted for the existing key.");
    } else if (slot == 3) {
        existingKey = pgpCard->authFpr();
    }
    if (!existingKey.empty()) {
        const QString message = i18nc("@info",
            "<p>This card already contains a key in this slot. Continuing will <b>overwrite</b> that key.</p>"
            "<p>If there is no backup the existing key will be irrecoverably lost.</p>") +
            i18n("The existing key has the fingerprint:") +
            QStringLiteral("<pre>%1</pre>").arg(QString::fromStdString(existingKey)) +
            encKeyWarning;
        const auto choice = KMessageBox::warningContinueCancel(parentWidgetOrView(), message,
            i18nc("@title:window", "Overwrite existing key"),
            KStandardGuiItem::cont(), KStandardGuiItem::cancel(), QString(), KMessageBox::Notify | KMessageBox::Dangerous);
        if (choice != KMessageBox::Continue) {
            finished();
            return;
        }
    }

    // Now do the deed
    const auto time = QDateTime::fromSecsSinceEpoch(subkey.creationTime());
    const auto timestamp = time.toString(QStringLiteral("yyyyMMdd'T'HHmmss"));
    const QString cmd = QStringLiteral("KEYTOCARD --force %1 %2 OPENPGP.%3 %4")
        .arg(QString::fromLatin1(subkey.keyGrip()), QString::fromStdString(serialNumber()))
        .arg(slot)
        .arg(timestamp);
    ReaderStatus::mutableInstance()->startSimpleTransaction(pgpCard, cmd.toUtf8(), q_func(), "keyToOpenPGPCardDone");
}

namespace {
static std::vector<Key> getEncryptionCertificates()
{
    std::vector<Key> encryptionCertificates = KeyCache::instance()->secretKeys();
    const auto it = std::remove_if(encryptionCertificates.begin(), encryptionCertificates.end(),
                                   [](const Key &key) {
                                       return ! (key.protocol() == GpgME::CMS &&
                                                 !key.subkey(0).isNull() &&
                                                 key.subkey(0).canEncrypt() &&
                                                 key.subkey(0).isSecret());
                                   });
    encryptionCertificates.erase(it, encryptionCertificates.end());
    return encryptionCertificates;
}
}

Subkey KeyToCardCommand::Private::getSubkeyToTransferToPIVCard(const std::string &cardSlot, const std::shared_ptr<PIVCard> &/*card*/)
{
    if (cardSlot != PIVCard::keyManagementKeyRef()) {
        return Subkey();
    }

    const std::vector<Key> encryptionCertificates = getEncryptionCertificates();
    if (encryptionCertificates.empty()) {
        error(i18n("Sorry! No suitable certificate to write to this card slot was found."));
        return Subkey();
    }

    auto dialog = new KeySelectionDialog(parentWidgetOrView());
    dialog->setWindowTitle(i18nc("@title:window", "Select Certificate"));
    dialog->setText(i18n("Please select the certificate whose key pair you want to write to the card:"));
    dialog->setKeys(encryptionCertificates);

    if (dialog->exec() == QDialog::Rejected) {
        return Subkey();
    }

    return dialog->selectedKey().subkey(0);
}

void KeyToCardCommand::Private::startKeyToPIVCard()
{
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::Private::startKeyToPIVCard()";

    const auto pivCard = SmartCard::ReaderStatus::instance()->getCard<PIVCard>(serialNumber());
    if (!pivCard) {
        error(i18n("Failed to find the PIV card with the serial number: %1", QString::fromStdString(serialNumber())));
        finished();
        return;
    }

    if (cardSlot != PIVCard::keyManagementKeyRef()) {
        // key to card is only supported for encryption keys
        finished();
        return;
    }

    if (subkey.isNull()) {
        subkey = getSubkeyToTransferToPIVCard(cardSlot, pivCard);
    }
    if (subkey.isNull()) {
        finished();
        return;
    }
    if (subkey.parent().protocol() != GpgME::CMS) {
        error(i18n("Sorry! This key cannot be transferred to a PIV card."));
        finished();
        return;
    }
    if (!subkey.canEncrypt() && !subkey.canSign()) {
        error(i18n("Sorry! Only encryption keys and signing keys can be transferred to a PIV card."));
        finished();
        return;
    }

    // Check if we need to do the overwrite warning.
    if (!overwriteExistingAlreadyApproved) {
        const std::string existingKey = pivCard->keyGrip(cardSlot);
        if (!existingKey.empty() && (existingKey != subkey.keyGrip())) {
            const QString decryptionWarning = (cardSlot == PIVCard::keyManagementKeyRef()) ?
                i18n("It will no longer be possible to decrypt past communication encrypted for the existing key.") :
                QString();
            const QString message = i18nc("@info",
                "<p>This card already contains a key in this slot. Continuing will <b>overwrite</b> that key.</p>"
                "<p>If there is no backup the existing key will be irrecoverably lost.</p>") +
                i18n("The existing key has the key grip:") +
                QStringLiteral("<pre>%1</pre>").arg(QString::fromStdString(existingKey)) +
                decryptionWarning;
            const auto choice = KMessageBox::warningContinueCancel(parentWidgetOrView(), message,
                i18nc("@title:window", "Overwrite existing key"),
                KStandardGuiItem::cont(), KStandardGuiItem::cancel(), QString(), KMessageBox::Notify | KMessageBox::Dangerous);
            if (choice != KMessageBox::Continue) {
                finished();
                return;
            }
            overwriteExistingAlreadyApproved = true;
        }
    }

    const QString cmd = QStringLiteral("KEYTOCARD --force %1 %2 %3")
        .arg(QString::fromLatin1(subkey.keyGrip()), QString::fromStdString(serialNumber()))
        .arg(QString::fromStdString(cardSlot));
    ReaderStatus::mutableInstance()->startSimpleTransaction(pivCard, cmd.toUtf8(), q_func(), "keyToPIVCardDone");
}

void KeyToCardCommand::Private::authenticate()
{
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::authenticate()";

    auto cmd = new AuthenticatePIVCardApplicationCommand(serialNumber(), parentWidgetOrView());
    connect(cmd, &AuthenticatePIVCardApplicationCommand::finished,
            q, [this]() { authenticationFinished(); });
    connect(cmd, &AuthenticatePIVCardApplicationCommand::canceled,
            q, [this]() { authenticationCanceled(); });
    cmd->start();
}

void KeyToCardCommand::Private::authenticationFinished()
{
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::authenticationFinished()";
    if (!hasBeenCanceled) {
        startKeyToPIVCard();
    }
}

void KeyToCardCommand::Private::authenticationCanceled()
{
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::authenticationCanceled()";
    hasBeenCanceled = true;
    canceled();
}

KeyToCardCommand::KeyToCardCommand(const GpgME::Subkey &key, const std::string &serialno)
    : CardCommand(new Private(this, key, serialno))
{
}

KeyToCardCommand::KeyToCardCommand(const std::string& cardSlot, const std::string &serialno)
    : CardCommand(new Private(this, cardSlot, serialno))
{
}

KeyToCardCommand::~KeyToCardCommand()
{
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::~KeyToCardCommand()";
}

bool KeyToCardCommand::supported()
{
    return true;
}

void KeyToCardCommand::keyToOpenPGPCardDone(const GpgME::Error &err)
{
    if (err) {
        d->error(i18nc("@info",
                       "Moving the key to the card failed: %1", QString::fromUtf8(err.asString())),
                        i18nc("@title", "Error"));
    } else if (!err.isCanceled()) {
        /* TODO DELETE_KEY is too strong, because it also deletes the stub
         * of the secret key. I could not find out how GnuPG does this. Question
         * to GnuPG Developers is pending an answer
        if (KMessageBox::questionYesNo(d->parentWidgetOrView(),
                                       i18n("Do you want to delete the key on this computer?"),
                                       i18nc("@title:window",
                                       "Key transferred to card")) == KMessageBox::Yes) {
            const QString cmd = QStringLiteral("DELETE_KEY --force %1").arg(d->subkey.keyGrip());
            // Using readerstatus is a bit overkill but it's an easy way to talk to the agent.
            ReaderStatus::mutableInstance()->startSimpleTransaction(card, cmd.toUtf8(), this, "deleteDone");
        }
        */
        d->information(i18nc("@info", "Successfully copied the key to the card."),
                       i18nc("@title", "Success"));
        ReaderStatus::mutableInstance()->updateStatus();
    }
    d->finished();
}

void KeyToCardCommand::keyToPIVCardDone(const GpgME::Error &err)
{
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::keyToPIVCardDone():"
                           << err.asString() << "(" << err.code() << ")";
    if (err) {
        // gpgme 1.13 reports "BAD PIN" instead of "NO AUTH"
        if (err.code() == GPG_ERR_NO_AUTH || err.code() == GPG_ERR_BAD_PIN) {
            d->authenticate();
            return;
        }

        d->error(i18nc("@info",
                       "Copying the key pair to the card failed: %1", QString::fromUtf8(err.asString())),
                        i18nc("@title", "Error"));
    } else if (!err.isCanceled()) {
        d->information(i18nc("@info", "Successfully copied the key pair to the card."),
                       i18nc("@title", "Success"));
        ReaderStatus::mutableInstance()->updateStatus();
    }

    d->finished();
}

void KeyToCardCommand::deleteDone(const GpgME::Error &err)
{
    if (err) {
        d->error(i18nc("@info", "Failed to delete the key: %1", QString::fromUtf8(err.asString())),
                        i18nc("@title", "Error"));
    }
    d->finished();
}

void KeyToCardCommand::doStart()
{
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::doStart()";

    d->start();
}

void KeyToCardCommand::doCancel()
{
}

#undef q_func
#undef d_func
