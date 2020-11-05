/*  SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "weboftrustwidget.h"

#include "commands/certifycertificatecommand.h"
#include "commands/revokecertificationcommand.h"

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

#include <QMenu>

using namespace Kleo;

class WebOfTrustWidget::Private
{
    friend class ::Kleo::WebOfTrustWidget;
    WebOfTrustWidget *const q;

private:
    GpgME::Key key;
    UserIDListModel certificationsModel;
    QGpgME::KeyListJob *keyListJob = nullptr;
    QTreeView *certificationsTV = nullptr;

public:
    Private(WebOfTrustWidget *qq)
        : q(qq)
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
        certificationsTV->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(certificationsTV, &QWidget::customContextMenuRequested,
                q, [this] (const QPoint &p) {
                    contextMenuRequested(p);
                });
    }

    void certificationDblClicked(const QModelIndex &idx)
    {
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

    void addActionsForUserID(QMenu *menu, const GpgME::UserID &userID)
    {
        menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-sign")),
                        i18n("Certify..."),
                        q, [this, userID]() {
            auto cmd = new Kleo::Commands::CertifyCertificateCommand(userID);
            cmd->setParentWidget(q);
            certificationsTV->setEnabled(false);
            connect(cmd, &Kleo::Commands::CertifyCertificateCommand::finished,
                    q, [this]() {
                certificationsTV->setEnabled(true);
                // Trigger an update when done
                q->setKey(key);
            });
            cmd->start();
        });
        if (Kleo::Commands::RevokeCertificationCommand::isSupported()) {
            menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-revoke")),
                            i18n("Revoke Certification..."),
                            q, [this, userID]() {
                auto cmd = new Kleo::Commands::RevokeCertificationCommand(userID);
                cmd->setParentWidget(q);
                certificationsTV->setEnabled(false);
                connect(cmd, &Kleo::Commands::RevokeCertificationCommand::finished,
                        q, [this]() {
                    certificationsTV->setEnabled(true);
                    // Trigger an update when done
                    q->setKey(key);
                });
                cmd->start();
            });
        }
    }

    void contextMenuRequested(const QPoint &p)
    {
        const auto index = certificationsTV->indexAt(p);
        const auto userID = certificationsModel.userID(index);
        const auto signature = certificationsModel.signature(index);

        if (userID.isNull() && signature.isNull()) {
            return;
        }

        QMenu *menu = new QMenu(q);
        if (!userID.isNull()) {
            addActionsForUserID(menu, userID);
        }
        connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
        menu->popup(certificationsTV->viewport()->mapToGlobal(p));
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
