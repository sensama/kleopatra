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

#include "weboftrustwidget.h"

#include <QHeaderView>
#include <QVBoxLayout>
#include <QTreeView>

#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>

#include <QGpgME/Protocol>
#include <QGpgME/KeyListJob>

#include <Libkleo/UserIDListModel>

#include "kleopatra_debug.h"
#include "commands/command.h"
#include "utils/remarks.h"

#include <KMessageBox>
#include <KLocalizedString>

using namespace Kleo;

class WebOfTrustWidget::Private
{
public:
    Private(WebOfTrustWidget *qq): keyListJob(nullptr), q(qq)
    {
        certificationsModel.enableRemarks(Remarks::remarksEnabled());

        certificationsTV = new QTreeView;
        certificationsTV->setModel(&certificationsModel);
        certificationsTV->setAllColumnsShowFocus(true);
        certificationsTV->setSelectionMode(QAbstractItemView::ExtendedSelection);

        auto vLay = new QVBoxLayout(q);
        vLay->setContentsMargins(0, 0, 0, 0);
        vLay->addWidget(certificationsTV);

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

        if (Remarks::remarksEnabled()) {
            job->addMode(GpgME::SignatureNotations);
        }


        /* Old style connect here again as QGPGME newstyle connects with
         * default arguments don't work on windows. */

        connect(job, &QGpgME::KeyListJob::result,
            q, &WebOfTrustWidget::signatureListingDone);

        connect(job, &QGpgME::KeyListJob::nextKey,
            q, &WebOfTrustWidget::signatureListingNextKey);

        job->start(QStringList(QString::fromLatin1(key.primaryFingerprint())));
        keyListJob = job;
    }

    GpgME::Key key;
    UserIDListModel certificationsModel;
    QGpgME::KeyListJob *keyListJob;
    QTreeView *certificationsTV;

private:
    WebOfTrustWidget *const q;
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

void WebOfTrustWidget::signatureListingNextKey(const GpgME::Key &key)
{
    GpgME::Key merged = key;
    merged.mergeWith(d->key);
    setKey(merged);
}

void WebOfTrustWidget::signatureListingDone(const GpgME::KeyListResult &result)
{
    if (result.error()) {
        KMessageBox::information(this, xi18nc("@info",
                                           "<para>An error occurred while loading the certifications: "
                                           "<message>%1</message></para>",
                                           QString::fromLocal8Bit(result.error().asString())),
                                 i18nc("@title", "Certifications Loading Failed"));
    }
    d->keyListJob = nullptr;
}

