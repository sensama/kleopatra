/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/csrcreationresultdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "csrcreationresultdialog.h"

#include <KConfigGroup>
#include <KGuiItem>
#include <KLocalizedString>
#include <KSeparator>
#include <KSharedConfig>
#include <KStandardGuiItem>

#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Kleo;
using namespace Kleo::Dialogs;

class CSRCreationResultDialog::Private
{
    friend class ::Kleo::Dialogs::CSRCreationResultDialog;
    CSRCreationResultDialog *const q;

    struct {
        QPlainTextEdit *csrBrowser = nullptr;
        QDialogButtonBox *buttonBox = nullptr;
    } ui;
    QByteArray csr;

public:
    Private(CSRCreationResultDialog *qq)
        : q(qq)
    {
        auto mainLayout = new QVBoxLayout(q);

        {
            auto label = new QLabel(i18n("The certificate signing request was created successfully. Please find the result and suggested next steps below."));
            label->setWordWrap(true);
            mainLayout->addWidget(label);
        }

        mainLayout->addWidget(new KSeparator(Qt::Horizontal));

        ui.csrBrowser = new QPlainTextEdit();
        ui.csrBrowser->setLineWrapMode(QPlainTextEdit::NoWrap);
        ui.csrBrowser->setReadOnly(true);
        ui.csrBrowser->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        mainLayout->addWidget(ui.csrBrowser);

        mainLayout->addWidget(new KSeparator(Qt::Horizontal));

        ui.buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
        KGuiItem::assign(ui.buttonBox->button(QDialogButtonBox::Close), KStandardGuiItem::close());
        connect(ui.buttonBox, &QDialogButtonBox::clicked, q, &QDialog::close);
        mainLayout->addWidget(ui.buttonBox);

        // calculate default size with enough space for the text edit
        const auto fm = ui.csrBrowser->fontMetrics();
        const QSize sizeHint = q->sizeHint();
        const QSize defaultSize = QSize(qMax(sizeHint.width(), 90 * fm.horizontalAdvance(QLatin1Char('x'))),
                                        sizeHint.height() - ui.csrBrowser->sizeHint().height() + 10 * fm.lineSpacing());
        restoreGeometry(defaultSize);
    }

    ~Private()
    {
        saveGeometry();
    }

private:
    void saveGeometry()
    {
        KConfigGroup cfgGroup(KSharedConfig::openConfig(), "CSRCreationResultDialog");
        cfgGroup.writeEntry("Size", q->size());
        cfgGroup.sync();
    }

    void restoreGeometry(const QSize &defaultSize)
    {
        KConfigGroup cfgGroup(KSharedConfig::openConfig(), "CSRCreationResultDialog");
        const QSize size = cfgGroup.readEntry("Size", defaultSize);
        if (size.isValid()) {
            q->resize(size);
        }
    }
};

CSRCreationResultDialog::CSRCreationResultDialog(QWidget *parent)
    : QDialog(parent)
    , d(new Private(this))
{
    setWindowTitle(i18nc("@title:window", "CSR Created"));
}

CSRCreationResultDialog::~CSRCreationResultDialog()
{
}

void CSRCreationResultDialog::setCSR(const QByteArray &csr)
{
    d->csr = csr;
    d->ui.csrBrowser->setPlainText(QString::fromLatin1(csr));
}
