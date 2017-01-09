/*  Copyright (c) 2016 Klarälvdalens Datakonsult AB

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

#include "subkeyswidget.h"
#include "ui_subkeyswidget.h"

#include <gpgme++/key.h>

#include <KConfigGroup>
#include <KSharedConfig>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTreeWidgetItem>

#include <libkleo/formatting.h>


class SubKeysWidget::Private
{
public:
    Private(SubKeysWidget *q)
        : q(q)
    {}

    GpgME::Key key;
    Ui::SubKeysWidget ui;

private:
    SubKeysWidget *q;
};

SubKeysWidget::SubKeysWidget(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{
    d->ui.setupUi(this);
}

SubKeysWidget::~SubKeysWidget()
{
}

void SubKeysWidget::setKey(const GpgME::Key &key)
{
    d->key = key;

    for (const auto &subkey : key.subkeys()) {
        auto item = new QTreeWidgetItem();
        item->setData(0, Qt::DisplayRole, QString::fromLatin1(subkey.keyID()));
        item->setData(1, Qt::DisplayRole, Kleo::Formatting::type(subkey));
        item->setData(2, Qt::DisplayRole, Kleo::Formatting::creationDateString(subkey));
        item->setData(3, Qt::DisplayRole, Kleo::Formatting::expirationDateString(subkey));
        item->setData(4, Qt::DisplayRole, Kleo::Formatting::validityShort(subkey));
        item->setData(5, Qt::DisplayRole, QString::number(subkey.length()));
        item->setData(6, Qt::DisplayRole, Kleo::Formatting::usageString(subkey));
        d->ui.subkeysTree->addTopLevelItem(item);
    }

    const auto subkey = key.subkey(0);
    if (const char *card = subkey.cardSerialNumber()) {
        d->ui.stored->setText(i18nc("stored...", "on SmartCard with serial no. %1", QString::fromUtf8(card)));
    } else {
        d->ui.stored->setText(i18nc("stored...", "on this computer"));
    }
}


GpgME::Key SubKeysWidget::key() const
{
    return d->key;
}



SubKeysDialog::SubKeysDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Subkeys details"));
    auto l = new QVBoxLayout(this);
    setLayout(l);
    l->addWidget(new SubKeysWidget(this));

    auto bbox = new QDialogButtonBox(this);
    auto btn = bbox->addButton(QDialogButtonBox::Close);
    connect(btn, &QPushButton::clicked, this, &QDialog::accept);
    l->addWidget(bbox);
    readConfig();
}

SubKeysDialog::~SubKeysDialog()
{
    writeConfig();
}

void SubKeysDialog::readConfig()
{
    KConfigGroup dialog(KSharedConfig::openConfig(), "SubKeysDialog");
    const QSize size = dialog.readEntry("Size", QSize(730, 280));
    if (size.isValid()) {
        resize(size);
    }
}

void SubKeysDialog::writeConfig()
{
    KConfigGroup dialog(KSharedConfig::openConfig(), "SubKeysDialog");
    dialog.writeEntry("Size", size());
    dialog.sync();
}

void SubKeysDialog::setKey(const GpgME::Key &key)
{
    auto w = findChild<SubKeysWidget*>();
    Q_ASSERT(w);
    w->setKey(key);
}

GpgME::Key SubKeysDialog::key() const
{
    auto w = findChild<SubKeysWidget*>();
    Q_ASSERT(w);
    return w->key();
}
