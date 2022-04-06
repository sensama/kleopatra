/* -*- mode: c++; c-basic-offset:4 -*-
    commands/exportopenpgpcerttoprovidercommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2022 Felix Tiede

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "exportopenpgpcerttoprovidercommand.h"
#include "command_p.h"

#include <gpgme++/key.h>
#include <QGpgME/Protocol>
#include <QGpgME/WKSPublishJob>

#include <kidentitymanagement/identity.h>
#include <kidentitymanagement/identitymanager.h>
#include <MailTransport/TransportManager>
#include <MailTransportAkonadi/MessageQueueJob>

#include <KLocalizedString>
#include <KMessageBox>

#include <QString>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;
using namespace QGpgME;

static const QString identityTransportForAddress(const QString &senderAddress) {
    static const KIdentityManagement::IdentityManager *idManager = new KIdentityManagement::IdentityManager{true};

    const KIdentityManagement::Identity identity = idManager->identityForAddress(senderAddress);
    if (identity.isNull()) {
        return idManager->defaultIdentity().transport();
    } else {
        return identity.transport();
    }
}

ExportOpenPGPCertToProviderCommand::ExportOpenPGPCertToProviderCommand(QAbstractItemView *v, KeyListController *c)
    : Command{v, c}
{
}

ExportOpenPGPCertToProviderCommand::ExportOpenPGPCertToProviderCommand(const UserID &uid)
    : Command{uid.parent()},
      uid{uid}
{
}

ExportOpenPGPCertToProviderCommand::~ExportOpenPGPCertToProviderCommand() = default;

void ExportOpenPGPCertToProviderCommand::doStart()
{
    const QString sender = senderAddress();
    const QString transportName = identityTransportForAddress(sender);

    if (transportName.isEmpty()) {
        KMessageBox::error(d->parentWidgetOrView(),
         xi18nc("@warning",
                "<para><email>%1</email> has no usable transport for mailing a key available, "
                "WKS upload not possible.</para>", sender),
         i18nc("@title:window", "OpenPGP Certificate Export"));
        d->canceled();
        return;
    }
    if (KMessageBox::warningContinueCancel(d->parentWidgetOrView(),
         xi18nc("@info",
                "<para>Not every mail provider supports WKS, so any key being "
                "exported this way may fail individually.</para><para>If exported, "
                "a confirmation request mail will be sent to <email>%1</email> "
                "which needs to be acknowledged with a mail program to complete the "
                "export process.</para><para><application>KMail</application> "
                "can handle these mails, but not all mail programs can.</para>"
                "<para>Once exported, the standard does not (yet) allow for "
                "automated removal of a published key.</para>"
                "<para>Are you sure you want to continue?</para>", sender),
         i18nc("@title:window", "OpenPGP Certificate Export"),
                KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                QStringLiteral("warn-export-openpgp-wks-unsupported"))
            == KMessageBox::Continue) {
        auto wksJob = QGpgME::openpgp()->wksPublishJob();
        connect(wksJob, &QGpgME::WKSPublishJob::result, this, &ExportOpenPGPCertToProviderCommand::wksJobResult);
        wksJob->startCreate(d->key().primaryFingerprint(), senderAddress());
    } else {
        d->canceled();
    }
}

void ExportOpenPGPCertToProviderCommand::doCancel()
{
}

void ExportOpenPGPCertToProviderCommand::wksJobResult(const GpgME::Error &error, const QByteArray &returnedData, const QByteArray &returnedError)
{
    if (error) {
        KMessageBox::error(d->parentWidgetOrView(),
         xi18nc("@error",
                "<para>An error occurred while trying to export OpenPGP certificates.</para> "
                "<para>The output from GnuPG WKS client was: <message>%1</message></para>",
                QString::fromUtf8(returnedError)),
         i18nc("@title:window", "OpenPGP Certificate Export"));
        d->canceled();
        return;
    }

    MailTransport::Transport *transport = MailTransport::TransportManager::self()->transportByName(
        identityTransportForAddress(senderAddress()));

    if (!transport) {
        d->canceled();
        return;
    }

    KMime::Message *msg = new KMime::Message;

    msg->setContent(KMime::CRLFtoLF(returnedData));
    msg->parse();

    MailTransport::MessageQueueJob *job = new MailTransport::MessageQueueJob(d->parentWidgetOrView());
    job->transportAttribute().setTransportId(transport->id());
    job->addressAttribute().setFrom(msg->from()->asUnicodeString());
    job->addressAttribute().setTo(msg->to()->displayNames());
    job->setMessage(KMime::Message::Ptr{msg});
    connect(job, &MailTransport::MessageQueueJob::result, this, [this](const KJob *mailJob) {
            if (mailJob->error()) {
                KMessageBox::error(d->parentWidgetOrView(),
                 xi18nc("@error",
                        "<para>An error occurred when creating the mail to publish key:</para>"
                        "<message>%1</message>", mailJob->errorString()),
                 i18nc("@title:window", "OpenPGP Certificate Export"));
                d->canceled();
            } else {
                d->finished();
            }
    });

    job->start();
}

QString ExportOpenPGPCertToProviderCommand::senderAddress() const
{
    if (uid.isNull()) {
        return QString::fromUtf8(d->key().userID(0).addrSpec().data());
    } else {
        return QString::fromUtf8(uid.addrSpec().data());
    }
}
