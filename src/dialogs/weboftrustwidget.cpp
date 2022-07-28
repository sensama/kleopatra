/*
    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "weboftrustwidget.h"

#include "commands/certifycertificatecommand.h"
#include "commands/revokecertificationcommand.h"
#include "utils/tags.h"

#include <Libkleo/KeyCache>
#include <Libkleo/NavigatableTreeView>
#include <Libkleo/UserIDListModel>

#include <KLocalizedString>
#include <KMessageBox>

#include <QHeaderView>
#include <QMenu>
#include <QVBoxLayout>

#include <QGpgME/KeyListJob>
#include <QGpgME/Protocol>

#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>

#include "kleopatra_debug.h"

using namespace Kleo;

class WebOfTrustWidget::Private
{
    friend class ::Kleo::WebOfTrustWidget;
    WebOfTrustWidget *const q;

private:
    GpgME::Key key;
    UserIDListModel certificationsModel;
    QGpgME::KeyListJob *keyListJob = nullptr;
    NavigatableTreeView *certificationsTV = nullptr;

public:
    Private(WebOfTrustWidget *qq)
        : q{qq}
    {
        certificationsModel.enableRemarks(Tags::tagsEnabled());

        certificationsTV = new NavigatableTreeView{q};
        certificationsTV->setModel(&certificationsModel);
        certificationsTV->setAllColumnsShowFocus(false);
        certificationsTV->setSelectionMode(QAbstractItemView::ExtendedSelection);
        if (!Tags::tagsEnabled()) {
            certificationsTV->hideColumn(static_cast<int>(UserIDListModel::Column::Tags));
        }

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

    void addActionsForSignature(QMenu *menu, const GpgME::UserID::Signature &signature)
    {
        menu->addAction(QIcon::fromTheme(QStringLiteral("dialog-information")),
                        i18n("Show Certificate Details..."),
                        q, [this, signature]() {
            auto cmd = Command::commandForQuery(QString::fromUtf8(signature.signerKeyID()));
            cmd->setParentWId(q->winId());
            cmd->start();
        });
        if (Kleo::Commands::RevokeCertificationCommand::isSupported()) {
            auto action = menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-revoke")),
                                          i18n("Revoke Certification..."),
                                          q, [this, signature]() {
                auto cmd = new Kleo::Commands::RevokeCertificationCommand(signature);
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
            const auto certificationKey = KeyCache::instance()->findByKeyIDOrFingerprint(signature.signerKeyID());
            const bool isSelfSignature = qstrcmp(signature.parent().parent().keyID(), signature.signerKeyID()) == 0;
            action->setEnabled(!isSelfSignature && certificationKey.hasSecret() && !signature.isRevokation() && !signature.isExpired() && !signature.isInvalid());
            if (isSelfSignature) {
                action->setToolTip(i18n("Revocation of self-certifications is currently not possible."));
            } else if (!certificationKey.hasSecret()) {
                action->setToolTip(i18n("You cannot revoke this certification because it wasn't made with one of your keys (or the required secret key is missing)."));
            } else if (signature.isRevokation()) {
                action->setToolTip(i18n("You cannot revoke this revocation certification. (But you can re-certify the corresponding user ID.)"));
            } else if (signature.isExpired()) {
                action->setToolTip(i18n("You cannot revoke this expired certification."));
            } else if (signature.isInvalid()) {
                action->setToolTip(i18n("You cannot revoke this invalid certification."));
            }
            if (!action->isEnabled()) {
                menu->setToolTipsVisible(true);
            }
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

        auto menu = new QMenu(q);
        if (!userID.isNull()) {
            addActionsForUserID(menu, userID);
        }
        else if (!signature.isNull()) {
            addActionsForSignature(menu, signature);
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

        if (Tags::tagsEnabled()) {
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


WebOfTrustWidget::WebOfTrustWidget(QWidget *parent)
    : QWidget{parent}
    , d{std::make_unique<Private>(this)}
{
}

WebOfTrustWidget::~WebOfTrustWidget() = default;

GpgME::Key WebOfTrustWidget::key() const
{
    return d->key;
}

void WebOfTrustWidget::setKey(const GpgME::Key &key)
{
    if (key.protocol() != GpgME::OpenPGP) {
        qCDebug(KLEOPATRA_LOG) << "List of Certifications is only supported for OpenPGP keys";
        return;
    }

    d->key = key;
    d->certificationsModel.setKey(key);
    d->certificationsTV->expandAll();
    d->certificationsTV->header()->resizeSections(QHeaderView::ResizeToContents);
    d->startSignatureListing();
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
