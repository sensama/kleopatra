/* commands/setinitialpincommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2017 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

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

#include "keytocardcommand.h"
#include "kleopatra_debug.h"

#include "command_p.h"

#include "smartcard/readerstatus.h"
#include "smartcard/openpgpcard.h"

#include <QInputDialog>
#include <QDateTime>
#include <QStringList>

#include <KLocalizedString>


#include <gpgme++/gpgmepp_version.h>
#if GPGMEPP_VERSION > 0x10801
# define GPGME_SUBKEY_HAS_KEYGRIP
#endif

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::SmartCard;
using namespace GpgME;

bool KeyToCardCommand::supported()
{
#ifdef GPGME_SUBKEY_HAS_KEYGRIP
    return true;
#else
    return false;
#endif
}

class KeyToCardCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::KeyToCardCommand;
    KeyToCardCommand *q_func() const
    {
        return static_cast<KeyToCardCommand *>(q);
    }
public:
    explicit Private(KeyToCardCommand *qq, const GpgME::Subkey &key, const std::string &serialno);
    ~Private();

private:
    void start()
    {
        // Check if we need to ask the user for the slot
        if ((mKey.canSign() || mKey.canCertify()) && !mKey.canEncrypt() && !mKey.canAuthenticate()) {
            // Signing only
            slotDetermined(1);
            return;
        }
        if (mKey.canEncrypt() && !(mKey.canSign() || mKey.canCertify()) && !mKey.canAuthenticate()) {
            // Encrypt only
            slotDetermined(2);
            return;
        }
        if (mKey.canAuthenticate() && !(mKey.canSign() || mKey.canCertify()) && !mKey.canEncrypt()) {
            // Auth only
            slotDetermined(3);
            return;
        }
        // Multiple uses, ask user.
        QStringList options;

        if (mKey.canSign() || mKey.canCertify()) {
            options << i18n("Signature") + QStringLiteral(" (1)");
        }
        if (mKey.canEncrypt()) {
            options << i18n("Encryption") + QStringLiteral(" (2)");
        }
        if (mKey.canAuthenticate()) {
            options << i18n("Authentication") + QStringLiteral(" (3)");
        }

        dialog = std::shared_ptr<QInputDialog> (new QInputDialog(parentWidgetOrView()));
        dialog->setComboBoxItems(options);

        connect(dialog.get(), &QDialog::rejected, q_func(), [this] () {finished();});
        connect(dialog.get(), &QInputDialog::textValueSelected, q_func(), [this] (const QString &text) {
                slotDetermined(text.at(text.size() - 1).digitValue());
            });
    }

    void slotDetermined(int slot)
    {
        // Check if we need to do the overwrite warning.
        const auto cards = ReaderStatus::instance()->getCards();

        qDebug() << "slot determined" << slot;
        bool cardFound = false;
        std::string existingKey;
        QString encKeyWarning;
        for (const auto &card: cards) {
            if (card->serialNumber() == mSerial) {
                const auto pgpCard = dynamic_cast<SmartCard::OpenPGPCard*>(card.get());
                Q_ASSERT(pgpCard);
                cardFound = true;
                if (slot == 1) {
                    existingKey = pgpCard->sigFpr();
                    break;
                }
                if (slot == 2) {
                    existingKey = pgpCard->encFpr();
                    encKeyWarning = i18n("It will no longer be possible to decrypt past communication "
                                         "encrypted for the existing key.");
                    break;
                }
                if (slot == 3) {
                    existingKey = pgpCard->authFpr();
                    break;
                }
                break;
            }
        }
        if (!cardFound) {
            error(i18n("Failed to find the card with the serial number: %1", QString::fromStdString(mSerial)));
            finished();
            return;
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
        const auto time = QDateTime::fromSecsSinceEpoch(mKey.creationTime());
        const auto timestamp = time.toString(QStringLiteral("yyyyMMdd'T'HHmmss"));
#ifdef GPGME_SUBKEY_HAS_KEYGRIP
        const QString cmd = QStringLiteral("KEYTOCARD --force %1 %2 OPENPGP.%3 %4").arg(QString::fromLatin1(mKey.keyGrip()), QString::fromStdString(mSerial))
                                                                                   .arg(slot)
                                                                                   .arg(timestamp);
        ReaderStatus::mutableInstance()->startSimpleTransaction(cmd.toUtf8(), q_func(), "keyToCardDone");
#else
        finished();
#endif
    }

private:
    std::shared_ptr<QInputDialog> dialog;
    std::string mSerial;
    GpgME::Subkey mKey;
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
            const QString cmd = QStringLiteral("DELETE_KEY --force %1").arg(d->mKey.keyGrip());
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

KeyToCardCommand::Private::Private(KeyToCardCommand *qq,
                                   const GpgME::Subkey &key,
                                   const std::string &serialno)
    : Command::Private(qq, nullptr),
      dialog(),
      mSerial(serialno),
      mKey(key)
{

}

KeyToCardCommand::Private::~Private() {}

KeyToCardCommand::KeyToCardCommand(const GpgME::Subkey &key, const std::string &serialno)
    : Command(new Private(this, key, serialno))
{
}

KeyToCardCommand::~KeyToCardCommand() {}

void KeyToCardCommand::doStart()
{
    d->start();
}

void KeyToCardCommand::doCancel()
{
    if (d->dialog) {
        d->dialog->close();
    }
}

#undef q_func
#undef d_func
