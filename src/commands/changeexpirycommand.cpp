/* -*- mode: c++; c-basic-offset:4 -*-
    commands/changeexpirycommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "changeexpirycommand.h"

#include "command_p.h"

#include <dialogs/expirydialog.h>

#include <Libkleo/Formatting>

#include <QGpgME/Protocol>
#include <QGpgME/ChangeExpiryJob>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include "kleopatra_debug.h"

#include <QDateTime>

#include <QDebug>

#include <gpgme++/gpgmepp_version.h>
#if GPGMEPP_VERSION >= 0x10E01 // 1.14.1
# define CHANGEEXPIRYJOB_SUPPORTS_SUBKEYS
#endif

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;
using namespace GpgME;
using namespace QGpgME;

class ChangeExpiryCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::ChangeExpiryCommand;
    ChangeExpiryCommand *q_func() const
    {
        return static_cast<ChangeExpiryCommand *>(q);
    }
public:
    explicit Private(ChangeExpiryCommand *qq, KeyListController *c);
    ~Private();

    void init();

private:
    void slotDialogAccepted();
    void slotDialogRejected();
    void slotResult(const Error &err);

private:
    void ensureDialogCreated();
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
    : Command::Private(qq, c),
      key(),
      dialog(),
      job()
{

}

ChangeExpiryCommand::Private::~Private()
{
    qCDebug(KLEOPATRA_LOG);
}

ChangeExpiryCommand::ChangeExpiryCommand(KeyListController *c)
    : Command(new Private(this, c))
{
    d->init();
}

ChangeExpiryCommand::ChangeExpiryCommand(QAbstractItemView *v, KeyListController *c)
    : Command(v, new Private(this, c))
{
    d->init();
}

ChangeExpiryCommand::ChangeExpiryCommand(const GpgME::Key &key)
    : Command(key, new Private(this, nullptr))
{
    d->init();
}

void ChangeExpiryCommand::Private::init()
{

}

ChangeExpiryCommand::~ChangeExpiryCommand()
{
    qCDebug(KLEOPATRA_LOG);
}

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

    const Subkey subkey = !d->subkey.isNull() ? d->subkey : d->key.subkey(0);

    d->ensureDialogCreated();
    Q_ASSERT(d->dialog);
    d->dialog->setDateOfExpiry(subkey.neverExpires() ? QDate() :
                               QDateTime::fromSecsSinceEpoch(subkey.expirationTime()).date());
    d->dialog->show();

}

void ChangeExpiryCommand::Private::slotDialogAccepted()
{
    Q_ASSERT(dialog);

    static const QTime END_OF_DAY(23, 59, 59);

    const QDateTime expiry(dialog->dateOfExpiry(), END_OF_DAY);

    qCDebug(KLEOPATRA_LOG) << "expiry" << expiry;

    createJob();
    Q_ASSERT(job);

    std::vector<Subkey> subkeys;
    if (!subkey.isNull()) {
        subkeys.push_back(subkey);
    }

#ifdef CHANGEEXPIRYJOB_SUPPORTS_SUBKEYS
    if (const Error err = job->start(key, expiry, subkeys)) {
#else
    if (const Error err = job->start(key, expiry)) {
#endif
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

void ChangeExpiryCommand::doCancel()
{
    qCDebug(KLEOPATRA_LOG);
    if (d->job) {
        d->job->slotCancel();
    }
}

void ChangeExpiryCommand::Private::ensureDialogCreated()
{
    if (dialog) {
        return;
    }

    dialog = new ExpiryDialog;
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
    connect(j, SIGNAL(result(GpgME::Error)),
            q, SLOT(slotResult(GpgME::Error)));

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

#undef d
#undef q

#include "moc_changeexpirycommand.cpp"
