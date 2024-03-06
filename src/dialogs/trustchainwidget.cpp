/*  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "trustchainwidget.h"
#include "ui_trustchainwidget.h"

#include "kleopatra_debug.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <gpgme++/key.h>

#include <Libkleo/Dn>
#include <Libkleo/KeyCache>

class TrustChainWidget::Private
{
public:
    Private(TrustChainWidget *qq)
        : q(qq)
    {
        Q_UNUSED(q);
    }

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
    const auto chain = Kleo::KeyCache::instance()->findIssuers(key, Kleo::KeyCache::RecursiveSearch | Kleo::KeyCache::IncludeSubject);
    if (chain.empty()) {
        return;
    }
    QTreeWidgetItem *last = nullptr;
    if (!chain.back().isRoot()) {
        last = new QTreeWidgetItem(d->ui.treeWidget);
        last->setText(0, i18n("Issuer Certificate Not Found (%1)", Kleo::DN(chain.back().issuerName()).prettyDN()));
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

#include "moc_trustchainwidget.cpp"
