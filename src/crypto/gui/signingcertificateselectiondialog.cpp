/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/signingcertificateselectiondialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "signingcertificateselectiondialog.h"
#include "signingcertificateselectionwidget.h"

#include <KLocalizedString>

#include <QMap>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>

using namespace Kleo;
using namespace Kleo::Crypto::Gui;

SigningCertificateSelectionDialog::SigningCertificateSelectionDialog(QWidget *parent)
    : QDialog(parent),
      widget(new SigningCertificateSelectionWidget(this))
{
    setWindowTitle(i18nc("@title:window", "Select Signing Certificates"));
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(widget);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SigningCertificateSelectionDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SigningCertificateSelectionDialog::reject);
    mainLayout->addWidget(buttonBox);
}

SigningCertificateSelectionDialog::~SigningCertificateSelectionDialog() {}

void SigningCertificateSelectionDialog::setSelectedCertificates(const QMap<GpgME::Protocol, GpgME::Key> &certificates)
{
    widget->setSelectedCertificates(certificates);
}

QMap<GpgME::Protocol, GpgME::Key> SigningCertificateSelectionDialog::selectedCertificates() const
{
    return widget->selectedCertificates();
}

bool SigningCertificateSelectionDialog::rememberAsDefault() const
{
    return widget->rememberAsDefault();
}

void SigningCertificateSelectionDialog::setAllowedProtocols(const QVector<GpgME::Protocol> &allowedProtocols)
{
    widget->setAllowedProtocols(allowedProtocols);
}

