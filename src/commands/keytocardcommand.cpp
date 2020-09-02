/* commands/setinitialpincommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "keytocardcommand.h"
#include "kleopatra_debug.h"

#include "command_p.h"

#include "smartcard/readerstatus.h"
#include "smartcard/openpgpcard.h"

#include <QInputDialog>
#include <QDateTime>
#include <QStringList>

#include <KLocalizedString>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::SmartCard;
using namespace GpgME;

class KeyToCardCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::KeyToCardCommand;
    KeyToCardCommand *q_func() const
    {
        return static_cast<KeyToCardCommand *>(q);
    }
public:
    explicit Private(KeyToCardCommand *qq, KeyListController *c);
    explicit Private(KeyToCardCommand *qq, const GpgME::Subkey &key, const std::string &serialno);
    ~Private();

private:
    void start();
    void slotDetermined(int slot);

private:
    std::string mSerial;
    GpgME::Subkey mSubkey;
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


KeyToCardCommand::Private::Private(KeyToCardCommand *qq, KeyListController *c)
    : Command::Private(qq, c)
{
}

KeyToCardCommand::Private::Private(KeyToCardCommand *qq,
                                   const GpgME::Subkey &key,
                                   const std::string &serialno)
    : Command::Private(qq, nullptr),
      mSerial(serialno),
      mSubkey(key)
{
}

KeyToCardCommand::Private::~Private()
{
}

void KeyToCardCommand::Private::start()
{
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::Private::start()";

    // Check if we need to ask the user for the slot
    if ((mSubkey.canSign() || mSubkey.canCertify()) && !mSubkey.canEncrypt() && !mSubkey.canAuthenticate()) {
        // Signing only
        slotDetermined(1);
        return;
    }
    if (mSubkey.canEncrypt() && !(mSubkey.canSign() || mSubkey.canCertify()) && !mSubkey.canAuthenticate()) {
        // Encrypt only
        slotDetermined(2);
        return;
    }
    if (mSubkey.canAuthenticate() && !(mSubkey.canSign() || mSubkey.canCertify()) && !mSubkey.canEncrypt()) {
        // Auth only
        slotDetermined(3);
        return;
    }
    // Multiple uses, ask user.
    QStringList options;

    if (mSubkey.canSign() || mSubkey.canCertify()) {
        options << i18nc("Placeholder is the number of a slot on a smart card", "Signature (%1)", 1);
    }
    if (mSubkey.canEncrypt()) {
        options << i18nc("Placeholder is the number of a slot on a smart card", "Encryption (%1)", 2);
    }
    if (mSubkey.canAuthenticate()) {
        options << i18nc("Placeholder is the number of a slot on a smart card", "Authentication (%1)", 3);
    }

    bool ok;
    const QString choice = QInputDialog::getItem(parentWidgetOrView(), i18n("Select Slot"),
        i18n("Please select the slot the key should be written to:"), options, /* current= */ 0, /* editable= */ false, &ok);
    const int slot = options.indexOf(choice) + 1;
    if (!ok || slot == 0) {
        finished();
    } else {
        slotDetermined(slot);
    }
}

void KeyToCardCommand::Private::slotDetermined(int slot)
{
    qCDebug(KLEOPATRA_LOG) << "KeyToCardCommand::Private::slotDetermined():" << slot;

    // Check if we need to do the overwrite warning.
    const auto pgpCard = ReaderStatus::instance()->getCard<OpenPGPCard>(mSerial);
    if (!pgpCard) {
        error(i18n("Failed to find the card with the serial number: %1", QString::fromStdString(mSerial)));
        finished();
        return;
    }

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
        if (KMessageBox::warningContinueCancel(parentWidgetOrView(), i18nc("@info",
            "<p>This card already contains a key in this slot. Continuing will <b>overwrite</b> that key.</p>"
            "<p>If there is no backup the existing key will be irrecoverably lost.</p>") +
            i18n("The existing key has the fingerprint:") + QStringLiteral("<pre>%1</pre>").arg(QString::fromStdString(existingKey)) +
            encKeyWarning,
            i18nc("@title:window", "Overwrite existing key"),
            KStandardGuiItem::cont(), KStandardGuiItem::cancel(), QString(), KMessageBox::Notify | KMessageBox::Dangerous)
            != KMessageBox::Continue) {
            finished();
            return;
        }
    }
    // Now do the deed
    const auto time = QDateTime::fromSecsSinceEpoch(mSubkey.creationTime());
    const auto timestamp = time.toString(QStringLiteral("yyyyMMdd'T'HHmmss"));
    const QString cmd = QStringLiteral("KEYTOCARD --force %1 %2 OPENPGP.%3 %4").arg(QString::fromLatin1(mSubkey.keyGrip()), QString::fromStdString(mSerial))
                                                                                .arg(slot)
                                                                                .arg(timestamp);
    ReaderStatus::mutableInstance()->startSimpleTransaction(cmd.toUtf8(), q_func(), "keyToCardDone");
}

KeyToCardCommand::KeyToCardCommand(KeyListController *c)
    : Command(new Private(this, c))
{
}

KeyToCardCommand::KeyToCardCommand(QAbstractItemView *v, KeyListController *c)
    : Command(v, new Private(this, c))
{
}

KeyToCardCommand::KeyToCardCommand(const GpgME::Key &key)
    : Command(key, new Private(this, nullptr))
{
}

KeyToCardCommand::KeyToCardCommand(const GpgME::Subkey &key, const std::string &serialno)
    : Command(new Private(this, key, serialno))
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

void KeyToCardCommand::keyToCardDone(const GpgME::Error &err)
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
            const QString cmd = QStringLiteral("DELETE_KEY --force %1").arg(d->mSubkey.keyGrip());
            // Using readerstatus is a bit overkill but it's an easy way to talk to the agent.
            ReaderStatus::mutableInstance()->startSimpleTransaction(cmd.toUtf8(), this, "deleteDone");
        }
        */
        KMessageBox::information(d->parentWidgetOrView(),
                                 i18n("Successfully copied the key to the card."),
                                 i18nc("@title", "Success"));
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

    if (d->mSubkey.isNull()) {
        const std::vector<Key> keys = d->keys();
        if (keys.size() != 1 ||
                !keys.front().hasSecret() ||
                keys.front().subkey(0).isNull()) {
            d->finished();
            return;
        }
        d->mSubkey = keys.front().subkey(0);
    }

    if (d->mSerial.empty()) {
        const auto cards = SmartCard::ReaderStatus::instance()->getCards();
        if (!cards.size() || cards[0]->serialNumber().empty()) {
            d->error(i18n("Failed to find a smart card."));
            d->finished();
            return;
        }
        const auto card = cards[0];
        if (card->appType() != SmartCard::Card::OpenPGPApplication) {
            d->error(i18n("Sorry! This OpenPGP key cannot be transferred to this non-OpenPGP card."));
            d->finished();
            return;
        }
        d->mSerial = card->serialNumber();
    }

    d->start();
}

void KeyToCardCommand::doCancel()
{
}

#undef q_func
#undef d_func
