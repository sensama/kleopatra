/*  SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "weboftrustdialog.h"
#include "weboftrustwidget.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include <KSharedConfig>
#include <KConfigGroup>

using namespace Kleo;

WebOfTrustDialog::WebOfTrustDialog(QWidget *parent)
    : QDialog(parent)
{
    KConfigGroup dialog(KSharedConfig::openStateConfig(), "WebOfTrustDialog");
    const QSize size = dialog.readEntry("Size", QSize(900, 400));
    if (size.isValid()) {
        resize(size);
    }
    setWindowTitle(i18nc("@title:window", "Certifications"));

    mWidget = new WebOfTrustWidget(this);
    auto l = new QVBoxLayout(this);
    l->addWidget(mWidget);

    auto bbox = new QDialogButtonBox(this);
    auto btn = bbox->addButton(QDialogButtonBox::Close);
    connect(btn, &QPushButton::pressed, this, &QDialog::accept);
    l->addWidget(bbox);
}

void WebOfTrustDialog::setKey(const GpgME::Key &key)
{
    mWidget->setKey(key);
}

GpgME::Key WebOfTrustDialog::key() const
{
    return mWidget->key();
}

WebOfTrustDialog::~WebOfTrustDialog()
{
    KConfigGroup dialog(KSharedConfig::openStateConfig(), "WebOfTrustDialog");
    dialog.writeEntry("Size", size());
    dialog.sync();
}
