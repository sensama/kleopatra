/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/chooseprotocolpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "chooseprotocolpage_p.h"

#include "utils/scrollarea.h"

#include <KLocalizedString>

#include <QCommandLinkButton>
#include <QVBoxLayout>

using namespace Kleo;
using namespace Kleo::NewCertificateUi;

namespace
{
static void force_set_checked(QAbstractButton *b, bool on)
{
    // work around Qt bug (tested: 4.1.4, 4.2.3, 4.3.4)
    const bool autoExclusive = b->autoExclusive();
    b->setAutoExclusive(false);
    b->setChecked(b->isEnabled() && on);
    b->setAutoExclusive(autoExclusive);
}
}

struct ChooseProtocolPage::UI {
    QCommandLinkButton *pgpCLB = nullptr;
    QCommandLinkButton *x509CLB = nullptr;

    UI(QWizardPage *parent)
    {
        parent->setTitle(i18nc("@title", "Choose Type of Key Pair"));
        parent->setSubTitle(i18n("Please choose which type of key pair you want to create."));

        auto mainLayout = new QVBoxLayout{parent};
        mainLayout->setContentsMargins(0, 0, 0, 0);

        auto scrollArea = new ScrollArea{parent};
        scrollArea->setFocusPolicy(Qt::NoFocus);
        scrollArea->setFrameStyle(QFrame::NoFrame);
        scrollArea->setBackgroundRole(parent->backgroundRole());
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto scrollAreaLayout = qobject_cast<QBoxLayout *>(scrollArea->widget()->layout());
        scrollAreaLayout->setContentsMargins(0, 0, 0, 0);

        pgpCLB = new QCommandLinkButton{parent};
        pgpCLB->setText(i18n("Create a personal OpenPGP key pair"));
        pgpCLB->setDescription(i18n("OpenPGP key pairs are certified by confirming the fingerprint of the public key."));
        pgpCLB->setAccessibleDescription(pgpCLB->description());
        pgpCLB->setCheckable(true);
        pgpCLB->setAutoExclusive(true);

        scrollAreaLayout->addWidget(pgpCLB);

        x509CLB = new QCommandLinkButton{parent};
        x509CLB->setText(i18n("Create a personal X.509 key pair and certification request"));
        x509CLB->setDescription(i18n("X.509 key pairs are certified by a certification authority (CA). The generated request needs to be sent to a CA to finalize creation."));
        x509CLB->setAccessibleDescription(x509CLB->description());
        x509CLB->setCheckable(true);
        x509CLB->setAutoExclusive(true);

        scrollAreaLayout->addWidget(x509CLB);

        scrollAreaLayout->addStretch(1);

        mainLayout->addWidget(scrollArea);
    }
};

ChooseProtocolPage::ChooseProtocolPage(QWidget *parent)
    : WizardPage{parent}
    , ui{new UI{this}}
{
    setObjectName(QStringLiteral("Kleo__NewCertificateUi__ChooseProtocolPage"));
    registerField(QStringLiteral("pgp"), ui->pgpCLB);
}

ChooseProtocolPage::~ChooseProtocolPage() = default;

void ChooseProtocolPage::setProtocol(GpgME::Protocol proto)
{
    if (proto == GpgME::OpenPGP) {
        ui->pgpCLB->setChecked(true);
    } else if (proto == GpgME::CMS) {
        ui->x509CLB->setChecked(true);
    } else {
        force_set_checked(ui->pgpCLB,  false);
        force_set_checked(ui->x509CLB, false);
    }
}

GpgME::Protocol ChooseProtocolPage::protocol() const
{
    return
        ui->pgpCLB->isChecked()  ? GpgME::OpenPGP :
        ui->x509CLB->isChecked() ? GpgME::CMS : GpgME::UnknownProtocol;
}

void ChooseProtocolPage::initializePage() {
    if (!initialized)
    {
        connect(ui->pgpCLB,  &QAbstractButton::clicked, wizard(), &QWizard::next, Qt::QueuedConnection);
        connect(ui->x509CLB, &QAbstractButton::clicked, wizard(), &QWizard::next, Qt::QueuedConnection);
    }
    initialized = true;
}

bool ChooseProtocolPage::isComplete() const
{
    return protocol() != GpgME::UnknownProtocol;
}

// #include "chooseprotocolpage.moc"
