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

#include "trustchainwidget.h"
#include "ui_trustchainwidget.h"

#include "kleopatra_debug.h"

#include <QTreeWidgetItem>
#include <QTreeWidget>
#include <QDialogButtonBox>
#include <QPushButton>

#include <gpgme++/key.h>

#include <Libkleo/Dn>
#include <Libkleo/KeyCache>

class TrustChainWidget::Private
{
public:
    Private(TrustChainWidget *qq)
        : q(qq)
    {}

    GpgME::Key key;
    Ui::TrustChainWidget ui;

private:
    TrustChainWidget *const q;
};

TrustChainWidget::TrustChainWidget(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{
    d->ui.setupUi(this);
}

TrustChainWidget::~TrustChainWidget()
{
}

void TrustChainWidget::setKey(const GpgME::Key &key)
{
    if (key.protocol() != GpgME::CMS) {
        qCDebug(KLEOPATRA_LOG) << "Trust chain is only supported for CMS keys";
        return;
    }

    d->key = key;
    d->ui.treeWidget->clear();
    const auto chain = Kleo::KeyCache::instance()->findIssuers(key,
                            Kleo::KeyCache::RecursiveSearch | Kleo::KeyCache::IncludeSubject);
    if (chain.empty()) {
        return;
    }
    QTreeWidgetItem *last = nullptr;
    if (!chain.back().isRoot()) {
        last = new QTreeWidgetItem(d->ui.treeWidget);
        last->setText(0, i18n("Issuer Certificate Not Found (%1)",
                              Kleo::DN(chain.back().issuerName()).prettyDN()));
        const QBrush &fg = d->ui.treeWidget->palette().brush(QPalette::Disabled, QPalette::WindowText);
        last->setForeground(0, fg);
    }
    for (auto it = chain.rbegin(), end = chain.rend(); it != end; ++it) {
        last = last ? new QTreeWidgetItem(last) : new QTreeWidgetItem(d->ui.treeWidget);
        last->setText(0, Kleo::DN(it->userID(0).id()).prettyDN());
    }
    d->ui.treeWidget->expandAll();
}

GpgME::Key TrustChainWidget::key() const
{
    return d->key;
}



TrustChainDialog::TrustChainDialog(QWidget *parent)
    : QDialog(parent)
{
    resize(650, 330);
    setWindowTitle(i18nc("@title:window", "Trust Chain"));

    auto l = new QVBoxLayout(this);
    l->addWidget(new TrustChainWidget(this));

    auto bbox = new QDialogButtonBox(this);
    auto btn = bbox->addButton(QDialogButtonBox::Close);
    connect(btn, &QPushButton::pressed, this, &QDialog::accept);
    l->addWidget(bbox);
}

TrustChainDialog::~TrustChainDialog()
{
}

void TrustChainDialog::setKey(const GpgME::Key &key)
{
    auto w = findChild<TrustChainWidget*>();
    Q_ASSERT(w);
    w->setKey(key);
}

GpgME::Key TrustChainDialog::key() const
{
    auto w = findChild<TrustChainWidget*>();
    Q_ASSERT(w);
    return w->key();
}

