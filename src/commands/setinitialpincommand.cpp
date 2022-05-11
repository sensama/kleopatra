/* -*- mode: c++; c-basic-offset:4 -*-
    commands/setinitialpincommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "setinitialpincommand.h"
#include "cardcommand_p.h"

#include "dialogs/setinitialpindialog.h"

#include "smartcard/netkeycard.h"
#include "smartcard/readerstatus.h"

#include <KLocalizedString>


using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;
using namespace Kleo::SmartCard;
using namespace GpgME;

class SetInitialPinCommand::Private : public CardCommand::Private
{
    friend class ::Kleo::Commands::SetInitialPinCommand;
    SetInitialPinCommand *q_func() const
    {
        return static_cast<SetInitialPinCommand *>(q);
    }
public:
    explicit Private(SetInitialPinCommand *qq, const std::string &serialNumber);
    ~Private() override;

private:
    void init()
    {
    }

    void ensureDialogCreated()
    {
        if (dialog) {
            return;
        }

        auto dlg = new SetInitialPinDialog;
        applyWindowID(dlg);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowTitle(i18nc("@title:window", "Set Initial Pin"));

        connect(dlg, SIGNAL(nksPinRequested()), q_func(), SLOT(slotNksPinRequested()));
        connect(dlg, SIGNAL(sigGPinRequested()), q_func(), SLOT(slotSigGPinRequested()));
        connect(dlg, SIGNAL(rejected()), q_func(), SLOT(slotDialogRejected()));
        connect(dlg, SIGNAL(accepted()), q_func(), SLOT(slotDialogAccepted()));

        dialog = dlg;
    }

    void ensureDialogVisible()
    {
        ensureDialogCreated();
        if (dialog->isVisible()) {
            dialog->raise();
        } else {
            dialog->show();
        }
    }

private:
    void setInitialPin(const char *pinRef, const char *resultSlot)
    {
        const auto nksCard = ReaderStatus::instance()->getCard<NetKeyCard>(serialNumber());
        if (!nksCard) {
            error(i18n("Failed to find the NetKey card with the serial number: %1", QString::fromStdString(serialNumber())));
            return;
        }

        const QByteArray command = QByteArray("SCD PASSWD --nullpin ") + pinRef;
        ReaderStatus::mutableInstance()->startSimpleTransaction(nksCard, command, dialog, resultSlot);
    }

    void slotNksPinRequested()
    {
        setInitialPin("PW1.CH", "setNksPinSettingResult");
    }

    void slotSigGPinRequested()
    {
        setInitialPin("PW1.CH.SIG", "setSigGPinSettingResult");
    }

    void slotDialogRejected()
    {
        if (dialog->isComplete()) {
            slotDialogAccepted();
        } else {
            canceled();
        }
    }
    void slotDialogAccepted()
    {
        ReaderStatus::mutableInstance()->updateStatus();
        finished();
    }

private:
    mutable QPointer<SetInitialPinDialog> dialog;
};

SetInitialPinCommand::Private *SetInitialPinCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const SetInitialPinCommand::Private *SetInitialPinCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define q q_func()
#define d d_func()

SetInitialPinCommand::Private::Private(SetInitialPinCommand *qq, const std::string &serialNumber)
    : CardCommand::Private(qq, serialNumber, nullptr),
      dialog()
{
}

SetInitialPinCommand::Private::~Private() {}

SetInitialPinCommand::SetInitialPinCommand(const std::string &serialNumber)
    : CardCommand(new Private(this, serialNumber))
{
    d->init();
}

SetInitialPinCommand::~SetInitialPinCommand() {}

QDialog *SetInitialPinCommand::dialog()
{
    d->ensureDialogCreated();
    return d->dialog;
}

void SetInitialPinCommand::doStart()
{
    d->ensureDialogCreated();

    const auto nksCard = ReaderStatus::instance()->getCard<NetKeyCard>(d->serialNumber());
    if (!nksCard) {
        d->error(i18n("Failed to find the NetKey card with the serial number: %1", QString::fromStdString(d->serialNumber())));
        d->dialog->close();
        finished();
        return;
    }

    const std::vector<Card::PinState> pinStates = nksCard->pinStates();
    d->dialog->setNksPinPresent(pinStates.size() >= 1 && pinStates[0] != Card::NullPin);
    d->dialog->setSigGPinPresent(pinStates.size() >= 3 && pinStates[2] != Card::NullPin);

    d->ensureDialogVisible();
}

void SetInitialPinCommand::doCancel()
{
    if (d->dialog) {
        d->dialog->close();
    }
}

#undef q_func
#undef d_func

#include "moc_setinitialpincommand.cpp"
