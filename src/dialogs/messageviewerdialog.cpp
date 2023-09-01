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

#include <memory>

namespace
{

/// Open first message from file
QVector<KMime::Message::Ptr> openFile(const QString &fileName)
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
            if (content.indexOf("From ", startOfMessage) == startOfMessage) {
                startOfMessage = content.indexOf('\n', startOfMessage) + 1;
            }
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
            startOfMessage = endOfMessage + 1;
            endOfMessage = content.indexOf("\nFrom ", startOfMessage);
        }
        if (endOfMessage == -1) {
            if (content.indexOf("From ", startOfMessage) == startOfMessage) {
                startOfMessage = content.indexOf('\n', startOfMessage) + 1;
            }
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
        return listMessages;
    }

    return {message};
}
}

class MessageViewerDialog::Private
{
public:
    int currentIndex = 0;
    QVector<KMime::Message::Ptr> messages;
    MimeTreeParser::Widgets::MessageViewer *messageViewer = nullptr;
    QPushButton *nextButton = nullptr;
    QPushButton *previousButton = nullptr;

    void setCurrentIndex(int currentIndex);
};

void MessageViewerDialog::Private::setCurrentIndex(int index)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < messages.count());
    Q_ASSERT(previousButton);
    Q_ASSERT(nextButton);

    currentIndex = index;
    messageViewer->setMessage(messages[currentIndex]);

    previousButton->setEnabled(currentIndex != 0);
    nextButton->setEnabled(currentIndex != messages.count() - 1);
}

MessageViewerDialog::MessageViewerDialog(const QString &fileName, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    const auto layout = new QVBoxLayout(this);

    d->messages += openFile(fileName);

    if (d->messages.isEmpty()) {
        auto errorMessage = new KMessageWidget(this);
        errorMessage->setMessageType(KMessageWidget::Error);
        errorMessage->setText(i18nc("@info", "Unable to read file"));
        layout->addWidget(errorMessage);
        return;
    }

    const bool multipleMessages = d->messages.length() > 1;
    if (multipleMessages) {
        auto hLayout = new QHBoxLayout();

        d->previousButton = new QPushButton(QIcon::fromTheme(QStringLiteral("go-previous")), i18nc("@action:button Previous email", "Previous Message"), this);
        d->previousButton->setEnabled(false);
        connect(d->previousButton, &QPushButton::clicked, this, [this](int index) {
            d->setCurrentIndex(d->currentIndex - 1);
        });

        d->nextButton = new QPushButton(QIcon::fromTheme(QStringLiteral("go-next")), i18nc("@action:button Next email", "Next Message"), this);
        connect(d->nextButton, &QPushButton::clicked, this, [this](int index) {
            d->setCurrentIndex(d->currentIndex + 1);
        });

        hLayout->addWidget(d->previousButton);
        hLayout->addStretch();
        hLayout->addWidget(d->nextButton);

        layout->addLayout(hLayout);
    }

    d->messageViewer = new MimeTreeParser::Widgets::MessageViewer(this);
    d->messageViewer->setMessage(d->messages[0]);
    layout->addWidget(d->messageViewer);

    auto buttonBox = new QDialogButtonBox(this);
    auto closeButton = buttonBox->addButton(QDialogButtonBox::Close);
    connect(closeButton, &QPushButton::pressed, this, &QDialog::accept);
    layout->addWidget(buttonBox);
}

MessageViewerDialog::~MessageViewerDialog() = default;
