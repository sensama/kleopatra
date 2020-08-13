/* -*- mode: c++; c-basic-offset:4 -*-
    commands/encryptclipboardcommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "encryptclipboardcommand.h"

#ifndef QT_NO_CLIPBOARD

#include "command_p.h"

#include <crypto/encryptemailcontroller.h>

#include <utils/input.h>
#include <utils/output.h>

#include <Libkleo/Stl_Util>

#include <KLocalizedString>
#include "kleopatra_debug.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>

#include <exception>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Crypto;

class EncryptClipboardCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::EncryptClipboardCommand;
    EncryptClipboardCommand *q_func() const
    {
        return static_cast<EncryptClipboardCommand *>(q);
    }
public:
    explicit Private(EncryptClipboardCommand *qq, KeyListController *c);
    ~Private();

    void init();

private:
    void slotRecipientsResolved();
    void slotControllerDone()
    {
        finished();
    }
    void slotControllerError(int, const QString &)
    {
        finished();
    }

private:
    std::shared_ptr<const ExecutionContext> shared_qq;
    std::shared_ptr<Input> input;
    EncryptEMailController controller;
};

EncryptClipboardCommand::Private *EncryptClipboardCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const EncryptClipboardCommand::Private *EncryptClipboardCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

EncryptClipboardCommand::Private::Private(EncryptClipboardCommand *qq, KeyListController *c)
    : Command::Private(qq, c),
      shared_qq(qq, [](EncryptClipboardCommand*){}),
      input(),
      controller(EncryptEMailController::ClipboardMode)
{

}

EncryptClipboardCommand::Private::~Private()
{
    qCDebug(KLEOPATRA_LOG);
}

EncryptClipboardCommand::EncryptClipboardCommand(KeyListController *c)
    : Command(new Private(this, c))
{
    d->init();
}

EncryptClipboardCommand::EncryptClipboardCommand(QAbstractItemView *v, KeyListController *c)
    : Command(v, new Private(this, c))
{
    d->init();
}

void EncryptClipboardCommand::Private::init()
{
    controller.setExecutionContext(shared_qq);
    connect(&controller, SIGNAL(done()), q, SLOT(slotControllerDone()));
    connect(&controller, SIGNAL(error(int,QString)), q, SLOT(slotControllerError(int,QString)));
}

EncryptClipboardCommand::~EncryptClipboardCommand()
{
    qCDebug(KLEOPATRA_LOG);
}

// static
bool EncryptClipboardCommand::canEncryptCurrentClipboard()
{
    if (const QClipboard *clip = QApplication::clipboard())
        if (const QMimeData *mime = clip->mimeData()) {
            return mime->hasText();
        }
    return false;
}

void EncryptClipboardCommand::doStart()
{

    try {

        // snapshot clipboard content here, in case it's being changed...
        d->input = Input::createFromClipboard();

        connect(&d->controller, SIGNAL(recipientsResolved()),
                this, SLOT(slotRecipientsResolved()));

        d->controller.startResolveRecipients();

    } catch (const std::exception &e) {
        d->information(i18n("An error occurred: %1",
                            QString::fromLocal8Bit(e.what())),
                       i18n("Encrypt Clipboard Error"));
        d->finished();
    }
}

void EncryptClipboardCommand::Private::slotRecipientsResolved()
{
    try {
        controller.setInputAndOutput(input, Output::createFromClipboard());
        input.reset(); // no longer needed, so don't keep a reference
        controller.start();
    } catch (const std::exception &e) {
        information(i18n("An error occurred: %1",
                         QString::fromLocal8Bit(e.what())),
                    i18n("Encrypt Clipboard Error"));
        finished();
    }
}

void EncryptClipboardCommand::doCancel()
{
    qCDebug(KLEOPATRA_LOG);
    d->controller.cancel();
}

#undef d
#undef q

#include "moc_encryptclipboardcommand.cpp"

#endif // QT_NO_CLIPBOARD
