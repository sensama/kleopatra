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

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Kleo;

class ChooseCertificateProtocolDialog::Private
{
    friend class ::Kleo::ChooseCertificateProtocolDialog;
    ChooseCertificateProtocolDialog *const q;

    struct UI {
        QPushButton *openpgpButton = nullptr;
        QPushButton *x509Button = nullptr;
        QDialogButtonBox *buttonBox = nullptr;

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

            {
                auto group = new QGroupBox{i18n("OpenPGP"), parent};
                group->setFlat(true);
                auto groupLayout = new QVBoxLayout{group};
                const auto infoText = i18n("OpenPGP key pairs are certified by confirming the fingerprint of the public key.");
                auto label = new QLabel{infoText, parent};
                label->setWordWrap(true);
                groupLayout->addWidget(label);
                openpgpButton = new QPushButton{parent};
                openpgpButton->setText(i18n("Create a Personal OpenPGP Key Pair"));
                openpgpButton->setAccessibleDescription(infoText);

                groupLayout->addWidget(openpgpButton);
                scrollAreaLayout->addWidget(group);
            }

            scrollAreaLayout->addWidget(new KSeparator{Qt::Horizontal, parent});

            {
                auto group = new QGroupBox{i18n("X.509"), parent};
                group->setFlat(true);
                auto groupLayout = new QVBoxLayout{group};
                const auto infoText = i18n("X.509 key pairs are certified by a certification authority (CA). The generated request needs to be sent to a CA to finalize creation.");
                auto label = new QLabel{infoText, parent};
                label->setWordWrap(true);
                groupLayout->addWidget(label);
                x509Button = new QPushButton{parent};
                x509Button->setText(i18n("Create a Personal X.509 Key Pair and Certification Request"));
                x509Button->setAccessibleDescription(infoText);

                groupLayout->addWidget(x509Button);
                scrollAreaLayout->addWidget(group);
            }

            mainLayout->addWidget(scrollArea);

            mainLayout->addStretch(1);

            mainLayout->addWidget(new KSeparator{Qt::Horizontal, parent});

            buttonBox = new QDialogButtonBox{QDialogButtonBox::Cancel, parent};
            buttonBox->button(QDialogButtonBox::Cancel)->setAutoDefault(false);

            mainLayout->addWidget(buttonBox);
        }
    } ui;

public:
    explicit Private(ChooseCertificateProtocolDialog *qq)
        : q{qq}
        , ui{qq}
    {
        q->setWindowTitle(i18nc("@title:window", "Choose Type of Key Pair"));

        connect(ui.openpgpButton,  &QAbstractButton::clicked, q, [this]() {
            protocol = GpgME::OpenPGP;
            q->accept();
        });
        connect(ui.x509Button,  &QAbstractButton::clicked, q, [this]() {
            protocol = GpgME::CMS;
            q->accept();
        });
        connect(ui.buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);
    }

private:
    GpgME::Protocol protocol = GpgME::UnknownProtocol;
};

ChooseCertificateProtocolDialog::ChooseCertificateProtocolDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog{parent, f}
    , d{new Private{this}}
{
}

ChooseCertificateProtocolDialog::~ChooseCertificateProtocolDialog() = default;

GpgME::Protocol ChooseCertificateProtocolDialog::protocol() const
{
    return d->protocol;
}

void ChooseCertificateProtocolDialog::showEvent(QShowEvent *event)
{
    // set WA_KeyboardFocusChange attribute to force visual focus of the
    // focussed command link button when the dialog is shown (required
    // for Breeze style and some other styles)
    window()->setAttribute(Qt::WA_KeyboardFocusChange);
    QDialog::showEvent(event);
}
