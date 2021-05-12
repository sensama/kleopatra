/*  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "certificatedetailsdialog.h"

#include "certificatedetailswidget.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <gpgme++/key.h>

CertificateDetailsDialog::CertificateDetailsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18nc("@title:window", "Certificate Details"));
    auto l = new QVBoxLayout(this);
    l->addWidget(new CertificateDetailsWidget(this));

    auto bbox = new QDialogButtonBox(this);
    auto btn = bbox->addButton(QDialogButtonBox::Close);
    connect(btn, &QPushButton::pressed, this, &QDialog::accept);
    l->addWidget(bbox);
    readConfig();
}

CertificateDetailsDialog::~CertificateDetailsDialog()
{
    writeConfig();
}

void CertificateDetailsDialog::readConfig()
{
    KConfigGroup dialog(KSharedConfig::openStateConfig(), "CertificateDetailsDialog");
    const QSize size = dialog.readEntry("Size", QSize(730, 280));
    if (size.isValid()) {
        resize(size);
    }
}

void CertificateDetailsDialog::writeConfig()
{
    KConfigGroup dialog(KSharedConfig::openStateConfig(), "CertificateDetailsDialog");
    dialog.writeEntry("Size", size());
    dialog.sync();
}

void CertificateDetailsDialog::setKey(const GpgME::Key &key)
{
    auto w = findChild<CertificateDetailsWidget*>();
    Q_ASSERT(w);
    w->setKey(key);
}

GpgME::Key CertificateDetailsDialog::key() const
{
    auto w = findChild<CertificateDetailsWidget*>();
    Q_ASSERT(w);
    return w->key();
}
