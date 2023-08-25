// This file is part of Kleopatra, the KDE keymanager
// SPDX-FileCopyrightText: 2023 g10 Code GmbH
// SPDX-FileContributor: Carl Schwan <carl.schwan@gnupg.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "messageviewerdialog.h"

#include "kleopatra_debug.h"

#include <KLocalizedString>
#include <KMessageWidget>
#include <KMime/Message>
#include <MimeTreeParserWidgets/MessageViewer>

#include <QDialogButtonBox>
#include <QFile>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{

/// Open first message from file
KMime::Message::Ptr openFile(const QString &fileName)
{
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(fileName);
    KMime::Message::Ptr message(new KMime::Message);

    QFile file(fileName);
    file.open(QIODevice::ReadOnly);
    if (!file.isOpen()) {
        qCWarning(KLEOPATRA_LOG) << "Could not open file";
        return {};
    }

    const auto content = file.readAll();

    if (content.length() == 0) {
        qCWarning(KLEOPATRA_LOG) << "File is empty";
        return {};
    }

    if (mime.inherits(QStringLiteral("application/pgp-encrypted")) || fileName.endsWith(QStringLiteral(".asc"))) {
        auto contentType = message->contentType();
        contentType->setMimeType("multipart/encrypted");
        contentType->setBoundary(KMime::multiPartBoundary());
        contentType->setParameter(QStringLiteral("protocol"), QStringLiteral("application/pgp-encrypted"));
        contentType->setCategory(KMime::Headers::CCcontainer);

        auto cte = message->contentTransferEncoding();
        cte->setEncoding(KMime::Headers::CE7Bit);
        cte->setDecoded(true);

        auto pgpEncrypted = new KMime::Content;
        pgpEncrypted->contentType()->setMimeType("application/pgp-encrypted");
        auto contentDisposition = new KMime::Headers::ContentDisposition;
        contentDisposition->setDisposition(KMime::Headers::CDattachment);
        pgpEncrypted->appendHeader(contentDisposition);
        pgpEncrypted->setBody("Version: 1");
        message->addContent(pgpEncrypted);

        auto encryptedContent = new KMime::Content;
        encryptedContent->contentType()->setMimeType("application/octet-stream");
        contentDisposition = new KMime::Headers::ContentDisposition;
        contentDisposition->setDisposition(KMime::Headers::CDinline);
        contentDisposition->setFilename(QStringLiteral("msg.asc"));
        encryptedContent->appendHeader(contentDisposition);
        encryptedContent->setBody(content);
        message->addContent(encryptedContent);

        message->assemble();
    } else {
        int startOfMessage = 0;
        if (content.startsWith("From ")) {
            startOfMessage = content.indexOf('\n');
            if (startOfMessage == -1) {
                return {};
            }
            startOfMessage += 1; // the message starts after the '\n'
        }
        QVector<KMime::Message::Ptr> listMessages;

        // check for multiple messages in the file
        int endOfMessage = content.indexOf("\nFrom ", startOfMessage);
        while (endOfMessage != -1) {
            auto msg = new KMime::Message;
            msg->setContent(KMime::CRLFtoLF(content.mid(startOfMessage, endOfMessage - startOfMessage)));
            msg->parse();
            if (!msg->hasContent()) {
                delete msg;
                msg = nullptr;
                return {};
            }
            KMime::Message::Ptr mMsg(msg);
            listMessages << mMsg;
            startOfMessage = content.indexOf('\n', endOfMessage + 1);
            endOfMessage = content.indexOf("\nFrom ", startOfMessage);
        }
        if (endOfMessage == -1) {
            endOfMessage = content.length();
            auto msg = new KMime::Message;
            msg->setContent(KMime::CRLFtoLF(content.mid(startOfMessage, endOfMessage - startOfMessage)));
            msg->parse();
            if (!msg->hasContent()) {
                delete msg;
                msg = nullptr;
                return {};
            }
            KMime::Message::Ptr mMsg(msg);
            listMessages << mMsg;
        }
        if (listMessages.count() > 0) {
            message = listMessages[0];
        }
    }

    return message;
}
}

MessageViewerDialog::MessageViewerDialog(const QString &fileName, QWidget *parent)
    : QDialog(parent)
{
    auto layout = new QVBoxLayout(this);

    auto message = openFile(fileName);

    if (!message) {
        auto errorMessage = new KMessageWidget(this);
        errorMessage->setMessageType(KMessageWidget::Error);
        errorMessage->setText(i18nc("@info", "Unable to read file"));
        layout->addWidget(errorMessage);
        return;
    }

    auto messageViewer = new MimeTreeParser::Widgets::MessageViewer(this);
    messageViewer->setMessage(message);
    layout->addWidget(messageViewer);

    auto buttonBox = new QDialogButtonBox(this);
    auto closeButton = buttonBox->addButton(QDialogButtonBox::Close);
    connect(closeButton, &QPushButton::pressed, this, &QDialog::accept);
    layout->addWidget(buttonBox);
}

MessageViewerDialog::~MessageViewerDialog() = default;
