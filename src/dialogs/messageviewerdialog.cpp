// This file is part of Kleopatra, the KDE keymanager
// SPDX-FileCopyrightText: 2023 g10 Code GmbH
// SPDX-FileContributor: Carl Schwan <carl.schwan@gnupg.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "messageviewerdialog.h"

#include <KMime/Message>
#include <MimeTreeParserWidgets/MessageViewer>

#include <QDialogButtonBox>
#include <QFile>
#include <QPushButton>
#include <QVBoxLayout>

MessageViewerDialog::MessageViewerDialog(const QString &fileName, QWidget *parent)
    : QDialog(parent)
{
    auto layout = new QVBoxLayout(this);

    QFile file(fileName);
    file.open(QIODevice::ReadOnly);
    const auto content = file.readAll();
    file.close();

    KMime::Message::Ptr message(new KMime::Message);
    message->setContent(content);
    message->parse();

    auto messageViewer = new MimeTreeParser::Widgets::MessageViewer(this);
    messageViewer->setMessage(message);
    layout->addWidget(messageViewer);

    auto buttonBox = new QDialogButtonBox(this);
    auto closeButton = buttonBox->addButton(QDialogButtonBox::Close);
    connect(closeButton, &QPushButton::pressed, this, &QDialog::accept);
    layout->addWidget(buttonBox);
}

MessageViewerDialog::~MessageViewerDialog() = default;
