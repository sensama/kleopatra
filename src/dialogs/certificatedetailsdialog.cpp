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
    auto l = new QVBoxLayout(this);
    auto w = new CertificateDetailsWidget{this};
    w->layout()->setContentsMargins(0, 0, 0, 0);
    l->addWidget(w);

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

namespace
{
QString title(const GpgME::Key &key)
{
    switch (key.protocol()) {
    case GpgME::OpenPGP:
        return i18nc("@title:window", "OpenPGP Certificate");
    case GpgME::CMS:
        return i18nc("@title:window", "S/MIME Certificate");
    default:
        return {};
    }
}
}

void CertificateDetailsDialog::setKey(const GpgME::Key &key)
{
    setWindowTitle(title(key));
    findChild<CertificateDetailsWidget *>()->setKey(key);
}

GpgME::Key CertificateDetailsDialog::key() const
{
    return findChild<CertificateDetailsWidget *>()->key();
}

#include "moc_certificatedetailsdialog.cpp"
