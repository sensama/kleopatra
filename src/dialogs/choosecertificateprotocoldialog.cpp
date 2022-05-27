/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/choosecertificateprotocoldialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "choosecertificateprotocoldialog.h"

#include "utils/scrollarea.h"

#include <KLocalizedString>
#include <KSeparator>

#include <QCommandLinkButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

using namespace Kleo;

class ChooseCertificateProtocolDialog::Private
{
    friend class ::Kleo::ChooseCertificateProtocolDialog;
    ChooseCertificateProtocolDialog *const q;

    struct UI {
        QCommandLinkButton *pgpCLB = nullptr;
        QCommandLinkButton *x509CLB = nullptr;

        UI(QDialog *parent)
        {
            auto mainLayout = new QVBoxLayout{parent};

            {
                auto label = new QLabel{i18n("Choose which type of key pair you want to create."), parent};
                label->setWordWrap(true);
                mainLayout->addWidget(label);
            }

            mainLayout->addWidget(new KSeparator{Qt::Horizontal, parent});

            auto scrollArea = new ScrollArea{parent};
            scrollArea->setFocusPolicy(Qt::NoFocus);
            scrollArea->setFrameStyle(QFrame::NoFrame);
            scrollArea->setBackgroundRole(parent->backgroundRole());
            scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            auto scrollAreaLayout = qobject_cast<QBoxLayout *>(scrollArea->widget()->layout());
            scrollAreaLayout->setContentsMargins(0, 0, 0, 0);

            pgpCLB = new QCommandLinkButton{parent};
            pgpCLB->setText(i18n("Create a Personal OpenPGP Key Pair"));
            pgpCLB->setDescription(i18n("OpenPGP key pairs are certified by confirming the fingerprint of the public key."));
            pgpCLB->setAccessibleDescription(pgpCLB->description());
            pgpCLB->setCheckable(true);
            pgpCLB->setAutoExclusive(true);

            scrollAreaLayout->addWidget(pgpCLB);

            x509CLB = new QCommandLinkButton{parent};
            x509CLB->setText(i18n("Create a Personal X.509 Key Pair and Certification Request"));
            x509CLB->setDescription(i18n("X.509 key pairs are certified by a certification authority (CA). The generated request needs to be sent to a CA to finalize creation."));
            x509CLB->setAccessibleDescription(x509CLB->description());
            x509CLB->setCheckable(true);
            x509CLB->setAutoExclusive(true);

            scrollAreaLayout->addWidget(x509CLB);

            mainLayout->addWidget(scrollArea);

            mainLayout->addStretch(1);

            mainLayout->addWidget(new KSeparator{Qt::Horizontal, parent});

            auto buttonBox = new QDialogButtonBox{QDialogButtonBox::Cancel, parent};

            mainLayout->addWidget(buttonBox);

            connect(pgpCLB,  &QAbstractButton::clicked, parent, &QDialog::accept);
            connect(x509CLB,  &QAbstractButton::clicked, parent, &QDialog::accept);
            connect(buttonBox, &QDialogButtonBox::rejected, parent, &QDialog::reject);
        }
    } ui;

public:
    explicit Private(ChooseCertificateProtocolDialog *qq)
        : q{qq}
        , ui{qq}
    {
        q->setWindowTitle(i18nc("@title:window", "Choose Type of Key Pair"));
    }
};

ChooseCertificateProtocolDialog::ChooseCertificateProtocolDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog{parent, f}
    , d{new Private{this}}
{
}

ChooseCertificateProtocolDialog::~ChooseCertificateProtocolDialog() = default;

GpgME::Protocol ChooseCertificateProtocolDialog::protocol() const
{
    return
        d->ui.pgpCLB->isChecked()  ? GpgME::OpenPGP :
        d->ui.x509CLB->isChecked() ? GpgME::CMS : GpgME::UnknownProtocol;
}

void ChooseCertificateProtocolDialog::showEvent(QShowEvent *event)
{
    // set WA_KeyboardFocusChange attribute to force visual focus of the
    // focussed command link button when the dialog is shown (required
    // for Breeze style and some other styles)
    window()->setAttribute(Qt::WA_KeyboardFocusChange);
    QDialog::showEvent(event);
}
