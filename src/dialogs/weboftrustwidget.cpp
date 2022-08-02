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
#include "utils/keys.h"
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
    QAction *detailsAction = nullptr;
    QAction *certifyAction = nullptr;
    QAction *revokeAction = nullptr;

public:
    Private(WebOfTrustWidget *qq)
        : q{qq}
    {
        certificationsModel.enableRemarks(Tags::tagsEnabled());

        certificationsTV = new NavigatableTreeView{q};
        certificationsTV->setAccessibleName(i18n("User IDs and certifications"));
        certificationsTV->setModel(&certificationsModel);
        certificationsTV->setAllColumnsShowFocus(false);
        certificationsTV->setSelectionMode(QAbstractItemView::SingleSelection);
        if (!Tags::tagsEnabled()) {
            certificationsTV->hideColumn(static_cast<int>(UserIDListModel::Column::Tags));
        }

        auto vLay = new QVBoxLayout(q);
        vLay->setContentsMargins(0, 0, 0, 0);
        vLay->addWidget(certificationsTV);

        detailsAction = new QAction{QIcon::fromTheme(QStringLiteral("dialog-information")), i18nc("@action", "Show Certificate Details"), q};
        connect(detailsAction, &QAction::triggered, q, [this]() {
            showCertificateDetails();
        });

        certifyAction = new QAction{QIcon::fromTheme(QStringLiteral("view-certificate-sign")), i18nc("@action", "Add Certification"), q};
        connect(certifyAction, &QAction::triggered, q, [this]() {
            addCertification();
        });

        if (Kleo::Commands::RevokeCertificationCommand::isSupported()) {
            revokeAction = new QAction{QIcon::fromTheme(QStringLiteral("view-certificate-revoke")), i18nc("@action", "Revoke Certification"), q};
            connect(revokeAction, &QAction::triggered, q, [this]() {
                revokeCertification();
            });
        }

        connect(certificationsTV, &QAbstractItemView::doubleClicked,
                q, [this]() {
                    certificationDblClicked();
                });
        certificationsTV->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(certificationsTV, &QWidget::customContextMenuRequested,
                q, [this] (const QPoint &p) {
                    contextMenuRequested(p);
                });
        connect(certificationsTV->selectionModel(), &QItemSelectionModel::currentRowChanged, q, [this]() {
            updateActions();
        });
        updateActions();
    }

    GpgME::UserID selectedUserID()
    {
        return certificationsModel.userID(certificationsTV->currentIndex());
    }

    GpgME::UserID::Signature selectedCertification()
    {
        return certificationsModel.signature(certificationsTV->currentIndex());
    }

    void certificationDblClicked()
    {
        showCertificateDetails();
    }

    void showCertificateDetails()
    {
        const auto signature = selectedCertification();
        if (signature.isNull()) {
            qCDebug(KLEOPATRA_LOG) << __func__ << "- no certification selected";
            return;
        }
        auto cmd = Command::commandForQuery(QString::fromUtf8(signature.signerKeyID()));
        cmd->setParentWId(q->winId());
        cmd->start();
    }

    void addCertification()
    {
        auto userID = selectedUserID();
        if (userID.isNull()) {
            userID = selectedCertification().parent();
        }
        if (userID.isNull()) {
            qCDebug(KLEOPATRA_LOG) << __func__ << "- no user ID or certification selected";
            return;
        }
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
    }

    void revokeCertification()
    {
        Command *cmd = nullptr;
        if (const auto signature = selectedCertification(); !signature.isNull()) {
            cmd = new Kleo::Commands::RevokeCertificationCommand(signature);
        } else if (const auto userID = selectedUserID(); !userID.isNull()) {
            cmd = new Kleo::Commands::RevokeCertificationCommand(userID);
        } else {
            qCDebug(KLEOPATRA_LOG) << __func__ << "- no user ID or certification selected";
            return;
        }
        cmd->setParentWidget(q);
        certificationsTV->setEnabled(false);
        connect(cmd, &Kleo::Commands::RevokeCertificationCommand::finished,
                q, [this]() {
            certificationsTV->setEnabled(true);
            // Trigger an update when done
            q->setKey(key);
        });
        cmd->start();
    }

    void addActionsForUserID(QMenu *menu)
    {
        menu->addAction(certifyAction);
        if (revokeAction) {
            menu->addAction(revokeAction);
        }
    }

    void addActionsForSignature(QMenu *menu)
    {
        menu->addAction(detailsAction);
        menu->addAction(certifyAction);
        if (revokeAction) {
            menu->addAction(revokeAction);
            if (!revokeAction->isEnabled()) {
                menu->setToolTipsVisible(true);
            }
        }
    }

    void updateActions() {
        const auto userCanSignUserIDs = userHasCertificationKey();
        const auto userID = selectedUserID();
        const auto signature = selectedCertification();
        detailsAction->setEnabled(!signature.isNull());
        certifyAction->setEnabled(userCanSignUserIDs && (!userID.isNull() || !signature.isNull()));
        if (revokeAction) {
            revokeAction->setToolTip({});
            if (!signature.isNull()) {
                const auto revocationFeasibility = userCanRevokeCertification(signature);
                revokeAction->setEnabled(revocationFeasibility == CertificationCanBeRevoked);
                switch (revocationFeasibility) {
                case CertificationCanBeRevoked:
                    break;
                case CertificationNotMadeWithOwnKey:
                    revokeAction->setToolTip(i18n("You cannot revoke this certification because it wasn't made with one of your keys (or the required secret key is missing)."));
                    break;
                case CertificationIsSelfSignature:
                    revokeAction->setToolTip(i18n("Revocation of self-certifications is currently not possible."));
                    break;
                case CertificationIsRevocation:
                    revokeAction->setToolTip(i18n("You cannot revoke this revocation certification. (But you can re-certify the corresponding user ID.)"));
                    break;
                case CertificationIsExpired:
                    revokeAction->setToolTip(i18n("You cannot revoke this expired certification."));
                    break;
                case CertificationIsInvalid:
                    revokeAction->setToolTip(i18n("You cannot revoke this invalid certification."));
                    break;
                case CertificationKeyNotAvailable:
                    revokeAction->setToolTip(i18n("You cannot revoke this certification because the required secret key is not available."));
                    break;
                };
            } else if (!userID.isNull()) {
                const bool canRevokeCertification = userCanRevokeCertifications(userID);
                revokeAction->setEnabled(canRevokeCertification);
                if (!canRevokeCertification) {
                    revokeAction->setToolTip(i18n("You cannot revoke any of the certifications of this user ID. Select any of the certifications for details."));
                }
            } else {
                revokeAction->setEnabled(false);
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
            addActionsForUserID(menu);
        }
        else if (!signature.isNull()) {
            addActionsForSignature(menu);
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

QAction *WebOfTrustWidget::detailsAction() const
{
    return d->detailsAction;
}

QAction *WebOfTrustWidget::certifyAction() const
{
    return d->certifyAction;
}

QAction *WebOfTrustWidget::revokeAction() const
{
    return d->revokeAction;
}

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
    d->updateActions();
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
