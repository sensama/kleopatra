/* -*- mode: c++; c-basic-offset:4 -*-
    commands/changeexpirycommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "changeexpirycommand.h"
#include "command_p.h"

#include "dialogs/expirydialog.h"

#include <Libkleo/Formatting>

#include <KLocalizedString>

#include <QGpgME/Protocol>
#include <QGpgME/ChangeExpiryJob>

#include <QDateTime>

#include <gpgme++/key.h>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;
using namespace GpgME;
using namespace QGpgME;

namespace
{
#ifdef QGPGME_SUPPORTS_CHANGING_EXPIRATION_OF_COMPLETE_KEY
bool allNotRevokedSubkeysHaveSameExpirationAsPrimaryKey(const Key &key)
{
    Q_ASSERT(!key.isNull() && key.numSubkeys() > 0);
    const auto subkeys = key.subkeys();
    const auto primaryKey = subkeys[0];
    if (primaryKey.neverExpires()) {
        return std::all_of(std::begin(subkeys), std::end(subkeys), [] (const auto &subkey) {
            // revoked subkeys are ignored by gpg --quick-set-expire when updating the expiration of all subkeys
            return subkey.isRevoked() || subkey.neverExpires();
        });
    }

    const auto primaryExpiration = primaryKey.expirationTime();
    return std::all_of(std::begin(subkeys), std::end(subkeys), [primaryExpiration] (const auto &subkey) {
        // revoked subkeys are ignored by gpg --quick-set-expire when updating the expiration of all subkeys;
        // check if expiration of subkey is (more or less) the same as the expiration of the primary key
        return subkey.isRevoked() ||
            (primaryExpiration - 10 <= subkey.expirationTime() && subkey.expirationTime() <= primaryExpiration + 10);
    });
}
#endif
}

class ChangeExpiryCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::ChangeExpiryCommand;
    ChangeExpiryCommand *q_func() const
    {
        return static_cast<ChangeExpiryCommand *>(q);
    }
public:
    explicit Private(ChangeExpiryCommand *qq, KeyListController *c);
    ~Private() override;

private:
    void slotDialogAccepted();
    void slotDialogRejected();
    void slotResult(const Error &err);

private:
    void ensureDialogCreated(ExpiryDialog::Mode mode);
    void createJob();
    void showErrorDialog(const Error &error);
    void showSuccessDialog();

private:
    GpgME::Key key;
    GpgME::Subkey subkey;
    QPointer<ExpiryDialog> dialog;
    QPointer<ChangeExpiryJob> job;
};

ChangeExpiryCommand::Private *ChangeExpiryCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const ChangeExpiryCommand::Private *ChangeExpiryCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

ChangeExpiryCommand::Private::Private(ChangeExpiryCommand *qq, KeyListController *c)
    : Command::Private{qq, c}
{
}

ChangeExpiryCommand::Private::~Private() = default;

void ChangeExpiryCommand::Private::slotDialogAccepted()
{
    Q_ASSERT(dialog);

    static const QTime END_OF_DAY{23, 59, 59};

    const QDateTime expiry{dialog->dateOfExpiry(), END_OF_DAY};

    qCDebug(KLEOPATRA_LOG) << "expiry" << expiry;

    createJob();
    Q_ASSERT(job);

#ifdef QGPGME_SUPPORTS_CHANGING_EXPIRATION_OF_COMPLETE_KEY
    if (subkey.isNull() && dialog->updateExpirationOfAllSubkeys()) {
        job->setOptions(ChangeExpiryJob::UpdateAllSubkeys);
    }
#endif

    std::vector<Subkey> subkeys;
    if (!subkey.isNull() && subkey.keyID() != key.keyID()) { // ignore the primary subkey
        subkeys.push_back(subkey);
    }

    if (const Error err = job->start(key, expiry, subkeys)) {
        showErrorDialog(err);
        finished();
    }
}

void ChangeExpiryCommand::Private::slotDialogRejected()
{
    Q_EMIT q->canceled();
    finished();
}

void ChangeExpiryCommand::Private::slotResult(const Error &err)
{
    if (err.isCanceled())
        ;
    else if (err) {
        showErrorDialog(err);
    } else {
        showSuccessDialog();
    }
    finished();
}

void ChangeExpiryCommand::Private::ensureDialogCreated(ExpiryDialog::Mode mode)
{
    if (dialog) {
        return;
    }

    dialog = new ExpiryDialog{mode};
    applyWindowID(dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, SIGNAL(accepted()), q, SLOT(slotDialogAccepted()));
    connect(dialog, SIGNAL(rejected()), q, SLOT(slotDialogRejected()));
}

void ChangeExpiryCommand::Private::createJob()
{
    Q_ASSERT(!job);

    const auto backend = (key.protocol() == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
    if (!backend) {
        return;
    }

    ChangeExpiryJob *const j = backend->changeExpiryJob();
    if (!j) {
        return;
    }

    connect(j, &Job::progress,
            q, &Command::progress);
    connect(j, &ChangeExpiryJob::result,
            q, [this] (const auto &err) { slotResult(err); });

    job = j;
}

void ChangeExpiryCommand::Private::showErrorDialog(const Error &err)
{
    error(i18n("<p>An error occurred while trying to change "
               "the expiry date for <b>%1</b>:</p><p>%2</p>",
               Formatting::formatForComboBox(key),
               QString::fromLocal8Bit(err.asString())),
          i18n("Expiry Date Change Error"));
}

void ChangeExpiryCommand::Private::showSuccessDialog()
{
    information(i18n("Expiry date changed successfully."),
                i18n("Expiry Date Change Succeeded"));
}

ChangeExpiryCommand::ChangeExpiryCommand(KeyListController *c)
    : Command{new Private{this, c}}
{
}

ChangeExpiryCommand::ChangeExpiryCommand(QAbstractItemView *v, KeyListController *c)
    : Command{v, new Private{this, c}}
{
}

ChangeExpiryCommand::ChangeExpiryCommand(const GpgME::Key &key)
    : Command{key, new Private{this, nullptr}}
{
}

ChangeExpiryCommand::~ChangeExpiryCommand() = default;

void ChangeExpiryCommand::setSubkey(const GpgME::Subkey &subkey)
{
    d->subkey = subkey;
}

void ChangeExpiryCommand::doStart()
{
    const std::vector<Key> keys = d->keys();
    if (keys.size() != 1 ||
            keys.front().protocol() != GpgME::OpenPGP ||
            !keys.front().hasSecret() ||
            keys.front().subkey(0).isNull()) {
        d->finished();
        return;
    }

    d->key = keys.front();

    if (!d->subkey.isNull() &&
            d->subkey.parent().primaryFingerprint() != d->key.primaryFingerprint()) {
        qDebug() << "Invalid subkey" << d->subkey.fingerprint()
                 << ": Not a subkey of key" << d->key.primaryFingerprint();
        d->finished();
        return;
    }

    ExpiryDialog::Mode mode;
    if (!d->subkey.isNull()) {
        mode = ExpiryDialog::Mode::UpdateIndividualSubkey;
    } else if (d->key.numSubkeys() == 1) {
        mode = ExpiryDialog::Mode::UpdateCertificateWithoutSubkeys;
    } else {
        mode = ExpiryDialog::Mode::UpdateCertificateWithSubkeys;
    }
    d->ensureDialogCreated(mode);
    Q_ASSERT(d->dialog);
    const Subkey subkey = !d->subkey.isNull() ? d->subkey : d->key.subkey(0);
    d->dialog->setDateOfExpiry(subkey.neverExpires() ? QDate() :
                               QDateTime::fromSecsSinceEpoch(subkey.expirationTime()).date());
#ifdef QGPGME_SUPPORTS_CHANGING_EXPIRATION_OF_COMPLETE_KEY
    if (mode == ExpiryDialog::Mode::UpdateCertificateWithSubkeys) {
        d->dialog->setUpdateExpirationOfAllSubkeys(allNotRevokedSubkeysHaveSameExpirationAsPrimaryKey(d->key));
    }
#endif

    d->dialog->show();

}

void ChangeExpiryCommand::doCancel()
{
    if (d->job) {
        d->job->slotCancel();
    }
}

#undef d
#undef q

#include "moc_changeexpirycommand.cpp"
