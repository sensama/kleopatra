/*  commands/setpivcardapplicationadministrationkeycommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "setpivcardapplicationadministrationkeycommand.h"

#include "cardcommand_p.h"

#include "smartcard/readerstatus.h"

#include "commands/authenticatepivcardapplicationcommand.h"

#include "dialogs/pivcardapplicationadministrationkeyinputdialog.h"

#include <KLocalizedString>

#include <gpgme++/error.h>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;
using namespace Kleo::SmartCard;
using namespace GpgME;

class SetPIVCardApplicationAdministrationKeyCommand::Private : public CardCommand::Private
{
    friend class ::Kleo::Commands::SetPIVCardApplicationAdministrationKeyCommand;
    SetPIVCardApplicationAdministrationKeyCommand *q_func() const
    {
        return static_cast<SetPIVCardApplicationAdministrationKeyCommand *>(q);
    }
public:
    explicit Private(SetPIVCardApplicationAdministrationKeyCommand *qq, const std::string &serialNumber, QWidget *p);
    ~Private();

    void init();

private:
    void slotDialogAccepted();
    void slotDialogRejected();
    void slotResult(const Error &err);

private:
    void authenticate();
    void authenticationFinished();
    void authenticationCanceled();
    void setAdminKey();
    void ensureDialogCreated();

private:
    QByteArray newAdminKey;
    QPointer<PIVCardApplicationAdministrationKeyInputDialog> dialog;
    bool hasBeenCanceled = false;
};

SetPIVCardApplicationAdministrationKeyCommand::Private *SetPIVCardApplicationAdministrationKeyCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const SetPIVCardApplicationAdministrationKeyCommand::Private *SetPIVCardApplicationAdministrationKeyCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

SetPIVCardApplicationAdministrationKeyCommand::Private::Private(SetPIVCardApplicationAdministrationKeyCommand *qq, const std::string &serialNumber, QWidget *p)
    : CardCommand::Private(qq, serialNumber, p)
    , dialog()
{
}

SetPIVCardApplicationAdministrationKeyCommand::Private::~Private()
{
    qCDebug(KLEOPATRA_LOG) << "SetPIVCardApplicationAdministrationKeyCommand::Private::~Private()";
}

SetPIVCardApplicationAdministrationKeyCommand::SetPIVCardApplicationAdministrationKeyCommand(const std::string &serialNumber, QWidget *p)
    : CardCommand(new Private(this, serialNumber, p))
{
    d->init();
}

void SetPIVCardApplicationAdministrationKeyCommand::Private::init()
{
}

SetPIVCardApplicationAdministrationKeyCommand::~SetPIVCardApplicationAdministrationKeyCommand()
{
    qCDebug(KLEOPATRA_LOG) << "SetPIVCardApplicationAdministrationKeyCommand::~SetPIVCardApplicationAdministrationKeyCommand()";
}

void SetPIVCardApplicationAdministrationKeyCommand::doStart()
{
    qCDebug(KLEOPATRA_LOG) << "SetPIVCardApplicationAdministrationKeyCommand::doStart()";

    d->authenticate();
}

void SetPIVCardApplicationAdministrationKeyCommand::doCancel()
{
}

void SetPIVCardApplicationAdministrationKeyCommand::Private::authenticate()
{
    qCDebug(KLEOPATRA_LOG) << "SetPIVCardApplicationAdministrationKeyCommand::authenticate()";

    auto cmd = new AuthenticatePIVCardApplicationCommand(serialNumber(), parentWidgetOrView());
    cmd->setPrompt(i18n("Please enter the old PIV Card Application Administration Key in hex-encoded form."));
    connect(cmd, &AuthenticatePIVCardApplicationCommand::finished,
            q, [this]() { authenticationFinished(); });
    connect(cmd, &AuthenticatePIVCardApplicationCommand::canceled,
            q, [this]() { authenticationCanceled(); });
    cmd->start();
}

void SetPIVCardApplicationAdministrationKeyCommand::Private::authenticationFinished()
{
    qCDebug(KLEOPATRA_LOG) << "SetPIVCardApplicationAdministrationKeyCommand::authenticationFinished()";
    if (!hasBeenCanceled) {
        setAdminKey();
    }
}

void SetPIVCardApplicationAdministrationKeyCommand::Private::authenticationCanceled()
{
    qCDebug(KLEOPATRA_LOG) << "SetPIVCardApplicationAdministrationKeyCommand::authenticationCanceled()";
    hasBeenCanceled = true;
    canceled();
}

void SetPIVCardApplicationAdministrationKeyCommand::Private::setAdminKey()
{
    qCDebug(KLEOPATRA_LOG) << "SetPIVCardApplicationAdministrationKeyCommand::setAdminKey()";

    ensureDialogCreated();
    Q_ASSERT(dialog);
    dialog->show();
}

void SetPIVCardApplicationAdministrationKeyCommand::Private::ensureDialogCreated()
{
    if (dialog) {
        return;
    }

    dialog = new PIVCardApplicationAdministrationKeyInputDialog(parentWidgetOrView());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setLabelText(newAdminKey.isEmpty() ?
                         i18n("Please enter the new PIV Card Application Administration Key in hex-encoded form. "
                              "The key needs to consist of 24 bytes, i.e. 48 hex-characters.") :
                         i18n("Please enter the new PIV Card Application Administration Key again."));

    connect(dialog, SIGNAL(accepted()), q, SLOT(slotDialogAccepted()));
    connect(dialog, SIGNAL(rejected()), q, SLOT(slotDialogRejected()));
}

void SetPIVCardApplicationAdministrationKeyCommand::Private::slotDialogAccepted()
{
    if (newAdminKey.isEmpty()) {
        newAdminKey = dialog->adminKey();
        dialog = nullptr;
        setAdminKey();
        return;
    }

    const QByteArray newAdminKey2 = dialog->adminKey();
    if (newAdminKey != newAdminKey2) {
        error(i18nc("@info", "The two keys you have entered do not match. Please retry."),
              i18nc("@title", "Error"));
        newAdminKey.clear();
        dialog = nullptr;
        setAdminKey();
        return;
    }

    const QByteArray plusPercentEncodedAdminKey = newAdminKey.toPercentEncoding().replace(' ', '+');
    const QByteArray command = QByteArray("SCD SETATTR SET-ADM-KEY ") + plusPercentEncodedAdminKey;
    ReaderStatus::mutableInstance()->startSimpleTransaction(command, q, "slotResult");
}

void SetPIVCardApplicationAdministrationKeyCommand::Private::slotDialogRejected()
{
    finished();
}

void SetPIVCardApplicationAdministrationKeyCommand::Private::slotResult(const GpgME::Error& err)
{
    qCDebug(KLEOPATRA_LOG) << "SetPIVCardApplicationAdministrationKeyCommand::slotResult():"
                           << err.asString() << "(" << err.code() << ")";
    if (err) {
        error(i18nc("@info", "Setting the PIV Card Application Administration Key failed: %1", QString::fromLatin1(err.asString())),
              i18nc("@title", "Error"));
    } else if (!err.isCanceled()) {
        information(i18nc("@info", "PIV Card Application Administration Key set successfully."), i18nc("@title", "Success"));
        ReaderStatus::mutableInstance()->updateStatus();
    }
    finished();
}

#undef d
#undef q

#include "moc_setpivcardapplicationadministrationkeycommand.cpp"
