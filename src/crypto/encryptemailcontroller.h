/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/encryptemailcontroller.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_CRYPTO_ENCRYPTEMAILCONTROLLER_H__
#define __KLEOPATRA_CRYPTO_ENCRYPTEMAILCONTROLLER_H__

#include <crypto/controller.h>

#include <utils/pimpl_ptr.h>

#include <gpgme++/global.h>

#include <memory>
#include <vector>

namespace KMime
{
namespace Types
{
class Mailbox;
}
}

namespace Kleo
{

class Input;
class Output;

namespace Crypto
{

class EncryptEMailController : public Controller
{
    Q_OBJECT
public:
    enum Mode {
        GpgOLMode,
        ClipboardMode,

        NumModes
    };

    explicit EncryptEMailController(Mode mode, QObject *parent = nullptr);
    explicit EncryptEMailController(const std::shared_ptr<ExecutionContext> &xc, Mode mode, QObject *parent = nullptr);
    ~EncryptEMailController() override;

    Mode mode() const;

    static const char *mementoName()
    {
        return "EncryptEMailController";
    }

    void setProtocol(GpgME::Protocol proto);
    const char *protocolAsString() const;
    GpgME::Protocol protocol() const;

    void startResolveRecipients();
    void startResolveRecipients(const std::vector<KMime::Types::Mailbox> &recipients, const std::vector<KMime::Types::Mailbox> &senders);

    void setInputAndOutput(const std::shared_ptr<Kleo::Input>   &input,
                           const std::shared_ptr<Kleo::Output> &output);
    void setInputsAndOutputs(const std::vector< std::shared_ptr<Kleo::Input> >   &inputs,
                             const std::vector< std::shared_ptr<Kleo::Output> > &outputs);

    void start();

public Q_SLOTS:
    void cancel();

Q_SIGNALS:
    void recipientsResolved();

private:

    void doTaskDone(const Task *task, const std::shared_ptr<const Kleo::Crypto::Task::Result> &) override;

    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotWizardRecipientsResolved())
    Q_PRIVATE_SLOT(d, void slotWizardCanceled())
    Q_PRIVATE_SLOT(d, void schedule())
};

} // Crypto
} // Kleo

#endif /* __KLEOPATRA_CRYPTO_ENCRYPTEMAILCONTROLLER_H__ */

