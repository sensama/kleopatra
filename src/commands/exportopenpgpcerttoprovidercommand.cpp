/* -*- mode: c++; c-basic-offset:4 -*-
    commands/exportopenpgpcerttoprovidercommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2019-2022 Felix Tiede

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "exportopenpgpcerttoprovidercommand.h"
#include "command_p.h"

#include <Libkleo/GnuPG>

#include <gpgme++/key.h>

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

static const QString identityTransportForAddress(const QString &senderAddress) {
    static const KIdentityManagement::IdentityManager *idManager = new KIdentityManagement::IdentityManager(true);

    const KIdentityManagement::Identity identity = idManager->identityForAddress(senderAddress);
    if (identity.isNull())
        return idManager->defaultIdentity().transport();
    else
        return identity.transport();
}

ExportOpenPGPCertToProviderCommand::ExportOpenPGPCertToProviderCommand(QAbstractItemView *v, KeyListController *c)
    : GnuPGProcessCommand(v, c)
{
    wksMail.open();
    wksMail.close();
}

ExportOpenPGPCertToProviderCommand::ExportOpenPGPCertToProviderCommand(const UserID &uid)
    : GnuPGProcessCommand(uid.parent()),
      uid(uid)
{
    wksMail.open();
    wksMail.close();
}

ExportOpenPGPCertToProviderCommand::~ExportOpenPGPCertToProviderCommand() {}

bool ExportOpenPGPCertToProviderCommand::preStartHook(QWidget *parent) const
{
    const QString sender = senderAddress();
    const QString transportName = identityTransportForAddress(sender);

    if (transportName.isEmpty()) {
        KMessageBox::error(parent,
         xi18nc("@warning",
                "<para><email>%1</email> has no usable transport for mailing a key available, "
                "WKS upload not possible.</para>", sender),
         i18nc("@title:window", "OpenPGP Certificate Export"));
        return false;
    }
    return KMessageBox::warningContinueCancel(parent,
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
            == KMessageBox::Continue;
}

void ExportOpenPGPCertToProviderCommand::postSuccessHook(QWidget *parent)
{
    MailTransport::Transport *transport = MailTransport::TransportManager::self()->transportByName(
        identityTransportForAddress(senderAddress()));

    if (!transport)
        return;

    wksMail.open();
    KMime::Message *msg = new KMime::Message();

    msg->setContent(KMime::CRLFtoLF(wksMail.readAll()));
    msg->parse();
    wksMail.close();

    MailTransport::MessageQueueJob *job = new MailTransport::MessageQueueJob(parent);
    job->transportAttribute().setTransportId(transport->id());
    job->addressAttribute().setFrom(msg->from()->asUnicodeString());
    job->addressAttribute().setTo(msg->to()->displayNames());
    job->setMessage(KMime::Message::Ptr(msg));
    connect(job, &MailTransport::MessageQueueJob::result, this, [this](const KJob *mailJob) {
            if (mailJob->error()) {
                KMessageBox::error((QWidget *) mailJob->parent(),
                 xi18nc("@error",
                        "<para>An error occurred when creating the mail to publish key:</para>"
                        "<para>%1</para>", mailJob->errorString()),
                 i18nc("@title:window", "OpenPGP Certificate Export"));
            }
    });

    job->start();
}

QStringList ExportOpenPGPCertToProviderCommand::arguments() const
{
    QStringList result;
    result << gpgWksClientPath();
    result << QStringLiteral("--output") << wksMail.fileName();
    result << QStringLiteral("--create");
    result << QString::fromUtf8(d->keys().at(0).primaryFingerprint());
    result << senderAddress();
    return result;
}

QString ExportOpenPGPCertToProviderCommand::errorCaption() const
{
    return i18nc("@title:window", "OpenPGP Certificate Export Error");
}

QString ExportOpenPGPCertToProviderCommand::successCaption() const
{
    return i18nc("@title:window", "OpenPGP Certificate Export Finished");
}

QString ExportOpenPGPCertToProviderCommand::crashExitMessage(const QStringList &args) const
{
    return xi18nc("@info",
                  "<para>The GPG process that tried to export OpenPGP certificates "
                  "ended prematurely because of an unexpected error.</para>"
                  "<para>Please check the output of <icode>%1</icode> for details.</para>", args.join(QLatin1Char(' ')));
}

QString ExportOpenPGPCertToProviderCommand::errorExitMessage(const QStringList &args) const
{
    return xi18nc("@info",
                  "<para>An error occurred while trying to export OpenPGP certificates.</para> "
                  "<para>The output from <command>%1</command> was: <message>%2</message></para>",
                  args[0], errorString());
}

QString ExportOpenPGPCertToProviderCommand::successMessage(const QStringList&) const
{
    return i18nc("@info", "OpenPGP certificates exported successfully.");
}

QString ExportOpenPGPCertToProviderCommand::senderAddress() const
{
    if (uid.isNull())
      return QString::fromUtf8(d->keys().at(0).userID(0).addrSpec().data());
    else
      return QString::fromUtf8(uid.addrSpec().data());
}
