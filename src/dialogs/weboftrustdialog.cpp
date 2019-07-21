/*  Copyright (c) 2017 Intevation GmbH

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
    KConfigGroup dialog(KSharedConfig::openConfig(), "WebOfTrustDialog");
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
    KConfigGroup dialog(KSharedConfig::openConfig(), "WebOfTrustDialog");
    dialog.writeEntry("Size", size());
    dialog.sync();
}
