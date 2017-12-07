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

#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QTreeView>

#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>

#include <QGpgME/Protocol>
#include <QGpgME/KeyListJob>

#include <Libkleo/UserIDListModel>

#include "weboftrustwidget.h"
#include "kleopatra_debug.h"
#include "commands/command.h"

#include <KMessageBox>
#include <KLocalizedString>

using namespace Kleo;

class WebOfTrustWidget::Private
{
public:
    Private(WebOfTrustWidget *qq): keyListJob(nullptr), q(qq)
    {
        certificationsTV = new QTreeView;
        certificationsTV->setModel(&certificationsModel);
        certificationsTV->setAllColumnsShowFocus(true);
        certificationsTV->setSelectionMode(QAbstractItemView::ExtendedSelection);

        auto vLay = new QVBoxLayout;
        vLay->addWidget(certificationsTV);

        q->setLayout(vLay);

        connect(certificationsTV, &QAbstractItemView::doubleClicked,
                q, [this] (const QModelIndex &idx) {
                certificationDblClicked(idx);
            });
    }

    void certificationDblClicked(const QModelIndex &idx) {
        if (!idx.isValid()) {
            return;
        }

        if (!idx.parent().isValid()) {
            // No parent -> root item.
            return;
        }

        // grab the keyid
        const auto query = certificationsModel.data(idx.sibling(idx.row(), 0)).toString();

        // Show details widget or search
        auto cmd = Command::commandForQuery(query);
        cmd->setParentWId(q->winId());
        cmd->start();
    }


    void startSignatureListing()
    {
        if (keyListJob) {
            return;
        }
        QGpgME::KeyListJob *const job = QGpgME::openpgp()->keyListJob(/*remote*/false, /*includeSigs*/true, /*validate*/true);
        if (!job) {
            return;
        }

        connect(job, &QGpgME::KeyListJob::result,
                q, [this](GpgME::KeyListResult result, std::vector<GpgME::Key>, QString, GpgME::Error) {
                signatureListingDone(result);
            });

        connect(job, &QGpgME::KeyListJob::nextKey,
                q, [this](const GpgME::Key &key) {
                signatureListingNextKey (key);
            });
        job->start(QStringList(QString::fromLatin1(key.primaryFingerprint())));
        keyListJob = job;
    }

    void signatureListingNextKey(const GpgME::Key &key)
    {
        GpgME::Key merged = key;
        merged.mergeWith(this->key);
        q->setKey(merged);
    }

    void signatureListingDone(const GpgME::KeyListResult &result)
    {
        if (result.error()) {
            KMessageBox::information(q, xi18nc("@info",
                                               "<para>An error occurred while loading the certifications: "
                                               "<message>%1</message></para>",
                                               QString::fromLocal8Bit(result.error().asString())),
                                     i18nc("@title", "Certifications Loading Failed"));
        }
        keyListJob = nullptr;
    }

    GpgME::Key key;
    UserIDListModel certificationsModel;
    QGpgME::KeyListJob *keyListJob;
    QTreeView *certificationsTV;

private:
    WebOfTrustWidget *q;
};


WebOfTrustWidget::WebOfTrustWidget(QWidget *parent) :
    QWidget(parent),
    d(new Private(this))
{
}

GpgME::Key WebOfTrustWidget::key() const
{
    return d->key;
}

void WebOfTrustWidget::setKey(const GpgME::Key &key)
{
    if (key.protocol() != GpgME::OpenPGP) {
        qCDebug(KLEOPATRA_LOG) << "Trust chain is only supported for CMS keys";
        return;
    }

    d->key = key;
    d->certificationsModel.setKey(key);
    d->certificationsTV->expandAll();
    d->certificationsTV->header()->resizeSections(QHeaderView::ResizeToContents);
    d->startSignatureListing();
}

WebOfTrustWidget::~WebOfTrustWidget()
{
}
