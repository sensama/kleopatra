/*  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "certificatedetailswidget.h"

#include "kleopatra_debug.h"
#include "exportdialog.h"
#include "trustchainwidget.h"
#include "subkeyswidget.h"
#include "weboftrustdialog.h"

#include "commands/changepassphrasecommand.h"
#include "commands/changeexpirycommand.h"
#include "commands/certifycertificatecommand.h"
#include "commands/revokecertificationcommand.h"
#include "commands/adduseridcommand.h"
#include "commands/genrevokecommand.h"
#include "commands/detailscommand.h"
#include "commands/dumpcertificatecommand.h"

#include "utils/tags.h"

#include <Libkleo/Formatting>
#include <Libkleo/Dn>
#include <Libkleo/KeyCache>

#include <KLocalizedString>

#include <gpgme++/context.h>
#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>
#include <gpgme++/tofuinfo.h>

#include <QGpgME/Protocol>
#include <QGpgME/KeyListJob>

#include <QDateTime>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QSpacerItem>
#include <QToolButton>
#include <QTreeWidget>

#include <set>

#include <gpgme++/gpgmepp_version.h>
#if GPGMEPP_VERSION >= 0x10E00 // 1.14.0
# define GPGME_HAS_REMARKS
#endif
#if GPGMEPP_VERSION >= 0x10F00 // 1.15.0
# define GPGME_HAS_WITH_SECRET
# include <QGpgME/Debug>
#endif

#define HIDE_ROW(row) \
    ui.row->setVisible(false); \
    ui.row##Lbl->setVisible(false);

#define SHOW_ROW(row) \
    ui.row->setVisible(true); \
    ui.row##Lbl->setVisible(true);

Q_DECLARE_METATYPE(GpgME::UserID)

using namespace Kleo;

class CertificateDetailsWidget::Private
{
public:
    Private(CertificateDetailsWidget *qq);

    void setupCommonProperties();
    void setupPGPProperties();
    void setupSMIMEProperties();

    void revokeUID(const GpgME::UserID &uid);
    void genRevokeCert();
    void certifyClicked();
    void webOfTrustClicked();
    void exportClicked();
    void addUserID();
    void changePassphrase();
    void changeExpiration();
    void keysMayHaveChanged();
    void showTrustChainDialog();
    void showMoreDetails();
    void publishCertificate();
    void userIDTableContextMenuRequested(const QPoint &p);

    QString tofuTooltipString(const GpgME::UserID &uid) const;

    void smimeLinkActivated(const QString &link);

    void setUpdatedKey(const GpgME::Key &key);
    void keyListDone(const GpgME::KeyListResult &,
                     const std::vector<GpgME::Key> &, const QString &,
                     const GpgME::Error &);

private:
    CertificateDetailsWidget *const q;

public:
    GpgME::Key key;
    bool updateInProgress = false;

private:
    struct UI {
        QGridLayout *gridLayout_2;
        QHBoxLayout *hboxLayout_1;
        QPushButton *addUserIDBtn;
        QPushButton *changePassphraseBtn;
        QPushButton *trustChainDetailsBtn;
        QPushButton *genRevokeBtn;
        QPushButton *certifyBtn;
        QGroupBox *groupBox;
        QGridLayout *gridLayout;
        QLabel *validFromLbl;
        QLabel *validFrom;
        QSpacerItem *horizontalSpacer_3;
        QLabel *expiresLbl;
        QHBoxLayout *horizontalLayout_3;
        QLabel *expires;
        QToolButton *changeExpirationBtn;
        QLabel *typeLbl;
        QLabel *type;
        QLabel *fingerprintLbl;
        QLabel *fingerprint;
        QLabel *publishingLbl;
        QPushButton *publishing;
        QLabel *smimeIssuerLbl;
        QLabel *smimeIssuer;
        QLabel *compliance;
        QLabel *complianceLbl;
        QLabel *trustedIntroducerLbl;
        QLabel *trustedIntroducer;
        QHBoxLayout *horizontalLayout;
        QPushButton *moreDetailsBtn;
        QPushButton *exportBtn;
        QPushButton *webOfTrustBtn;
        QSpacerItem *horizontalSpacer;
        QTreeWidget *userIDTable;
        QLabel *label;
        QLabel *smimeOwnerLbl;
        QLabel *smimeRelatedAddresses;
        QLabel *smimeOwner;

        void setupUi(QWidget *parent)
        {
            gridLayout_2 = new QGridLayout(parent);
            gridLayout_2->setContentsMargins(0, 0, 0, 0);
            hboxLayout_1 = new QHBoxLayout();
            addUserIDBtn = new QPushButton(i18n("Add email address"), parent);

            hboxLayout_1->addWidget(addUserIDBtn);

            changePassphraseBtn = new QPushButton(i18n("Change passphrase"), parent);

            hboxLayout_1->addWidget(changePassphraseBtn);

            trustChainDetailsBtn = new QPushButton(i18n("Trust chain details..."), parent);

            hboxLayout_1->addWidget(trustChainDetailsBtn);

            genRevokeBtn = new QPushButton(i18n("Generate revocation certificate"), parent);
            genRevokeBtn->setToolTip(u"<html>" +
                                     i18n("A revocation certificate is a file that serves as a \"kill switch\" to publicly "
                                          "declare that a key shall not anymore be used.  It is not possible "
                                          "to retract such a revocation certificate once it has been published.") +
                                     u"</html>");

            hboxLayout_1->addWidget(genRevokeBtn);

            certifyBtn = new QPushButton(i18n("Certify"), parent);

            hboxLayout_1->addWidget(certifyBtn);

            gridLayout_2->addLayout(hboxLayout_1, 4, 0, 1, 3);

            groupBox = new QGroupBox(i18n("Certificate Details"), parent);
            groupBox->setFlat(false);
            gridLayout = new QGridLayout(groupBox);
            int row = 0;
            validFromLbl = new QLabel(i18n("Valid from:"), groupBox);

            gridLayout->addWidget(validFromLbl, row, 0, 1, 1);

            validFrom = new QLabel(groupBox);
            validFrom->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

            gridLayout->addWidget(validFrom, row, 1, 1, 1);

            horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

            gridLayout->addItem(horizontalSpacer_3, row, 2, 1, 1);
            row++;

            expiresLbl = new QLabel(i18n("Expires:"), groupBox);

            gridLayout->addWidget(expiresLbl, row, 0, 1, 1);

            horizontalLayout_3 = new QHBoxLayout();
            expires = new QLabel(groupBox);
            expires->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

            horizontalLayout_3->addWidget(expires);

            changeExpirationBtn = new QToolButton(groupBox);
            changeExpirationBtn->setToolTip(i18n("Change"));
            changeExpirationBtn->setIcon(QIcon::fromTheme(QStringLiteral("editor")));
            changeExpirationBtn->setIconSize(QSize(16, 16));
            changeExpirationBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

            horizontalLayout_3->addWidget(changeExpirationBtn);

            gridLayout->addLayout(horizontalLayout_3, row, 1, 1, 1);
            row++;

            typeLbl = new QLabel(i18n("Type:"), groupBox);

            gridLayout->addWidget(typeLbl, row, 0, 1, 1);

            type = new QLabel(groupBox);
            type->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

            gridLayout->addWidget(type, row, 1, 1, 1);
            row++;

            fingerprintLbl = new QLabel(i18n("Fingerprint:"), groupBox);

            gridLayout->addWidget(fingerprintLbl, row, 0, 1, 1);

            fingerprint = new QLabel(groupBox);
            fingerprint->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

            gridLayout->addWidget(fingerprint, row, 1, 1, 2);
            row++;

            publishingLbl = new QLabel(i18n("Publishing:"), groupBox);

            gridLayout->addWidget(publishingLbl, row, 0, 1, 1);

            publishing = new QPushButton(i18n("Publish Certificate"), groupBox);

            gridLayout->addWidget(publishing, row, 1, 1, 1);
            row++;

            smimeIssuerLbl = new QLabel(i18n("Issuer:"), groupBox);

            gridLayout->addWidget(smimeIssuerLbl, row, 0, 1, 1);

            smimeIssuer = new QLabel(groupBox);
            smimeIssuer->setWordWrap(true);
            smimeIssuer->setTextInteractionFlags(Qt::TextBrowserInteraction);

            gridLayout->addWidget(smimeIssuer, row, 1, 1, 2);
            row++;

            compliance = new QLabel(i18n("Compliance:"), groupBox);
            compliance->setWordWrap(true);
            compliance->setTextInteractionFlags(Qt::TextBrowserInteraction);

            gridLayout->addWidget(compliance, row, 0, 1, 1);

            complianceLbl = new QLabel(groupBox);
            complianceLbl->setWordWrap(true);
            complianceLbl->setTextInteractionFlags(Qt::TextBrowserInteraction);

            gridLayout->addWidget(complianceLbl, row, 1, 1, 2);
            row++;

            trustedIntroducerLbl = new QLabel(i18n("Trusted introducer for:"), groupBox);
            trustedIntroducerLbl->setToolTip(i18n("See certifications for details."));
            trustedIntroducerLbl->setTextInteractionFlags(Qt::TextBrowserInteraction);

            gridLayout->addWidget(trustedIntroducerLbl, row, 0, 1, 1);

            trustedIntroducer = new QLabel(groupBox);
            trustedIntroducer->setWordWrap(true);
            trustedIntroducer->setToolTip(i18n("See certifications for details."));
            trustedIntroducer->setTextInteractionFlags(Qt::TextBrowserInteraction);

            gridLayout->addWidget(trustedIntroducer, row, 1, 1, 2);
            row++;

            horizontalLayout = new QHBoxLayout();
            moreDetailsBtn = new QPushButton(i18n("More details..."), groupBox);

            horizontalLayout->addWidget(moreDetailsBtn);

            exportBtn = new QPushButton(i18n("Export..."), groupBox);

            horizontalLayout->addWidget(exportBtn);

            webOfTrustBtn = new QPushButton(i18n("Certifications..."), groupBox);

            horizontalLayout->addWidget(webOfTrustBtn);

            horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

            horizontalLayout->addItem(horizontalSpacer);

            gridLayout->addLayout(horizontalLayout, row, 0, 1, 3);

            gridLayout_2->addWidget(groupBox, 6, 0, 1, 3);

            userIDTable = new QTreeWidget(parent);
            QTreeWidgetItem *__qtreewidgetitem = new QTreeWidgetItem();
            __qtreewidgetitem->setText(0, QString::fromUtf8("1"));
            userIDTable->setHeaderItem(__qtreewidgetitem);
            userIDTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
            userIDTable->setSelectionMode(QAbstractItemView::SingleSelection);
            userIDTable->setRootIsDecorated(false);
            userIDTable->setUniformRowHeights(true);
            userIDTable->setAllColumnsShowFocus(true);

            gridLayout_2->addWidget(userIDTable, 3, 0, 1, 3);

            label = new QLabel(i18n("You can use this certificate to secure communication with the following email addresses:"), parent);
            label->setWordWrap(true);

            gridLayout_2->addWidget(label, 0, 0, 1, 3);

            smimeOwnerLbl = new QLabel(i18n("Owner:"), parent);

            gridLayout_2->addWidget(smimeOwnerLbl, 1, 0, 1, 1);

            smimeRelatedAddresses = new QLabel(i18n("Related addresses:"), parent);
            QFont font;
            font.setBold(true);
            font.setWeight(75);
            smimeRelatedAddresses->setFont(font);

            gridLayout_2->addWidget(smimeRelatedAddresses, 2, 0, 1, 1);

            smimeOwner = new QLabel(parent);
            smimeOwner->setWordWrap(true);
            smimeOwner->setTextInteractionFlags(Qt::TextBrowserInteraction);

            gridLayout_2->addWidget(smimeOwner, 1, 1, 1, 2);
        }
    } ui;
};

CertificateDetailsWidget::Private::Private(CertificateDetailsWidget *qq)
    : q{qq}
{
    ui.setupUi(q);

    connect(ui.addUserIDBtn, &QPushButton::clicked,
            q, [this]() { addUserID(); });
    connect(ui.changePassphraseBtn, &QPushButton::clicked,
            q, [this]() { changePassphrase(); });
    connect(ui.genRevokeBtn, &QPushButton::clicked,
            q, [this]() { genRevokeCert(); });
    connect(ui.changeExpirationBtn, &QPushButton::clicked,
            q, [this]() { changeExpiration(); });
    connect(ui.smimeOwner, &QLabel::linkActivated,
            q, [this](const QString &link) { smimeLinkActivated(link); });
    connect(ui.smimeIssuer, &QLabel::linkActivated,
            q, [this](const QString &link) { smimeLinkActivated(link); });
    connect(ui.trustChainDetailsBtn, &QPushButton::pressed,
            q, [this]() { showTrustChainDialog(); });
    connect(ui.moreDetailsBtn, &QPushButton::pressed,
            q, [this]() { showMoreDetails(); });
    connect(ui.publishing, &QPushButton::pressed,
            q, [this]() { publishCertificate(); });
    connect(ui.certifyBtn, &QPushButton::clicked,
            q, [this]() { certifyClicked(); });
    connect(ui.webOfTrustBtn, &QPushButton::clicked,
            q, [this]() { webOfTrustClicked(); });
    connect(ui.exportBtn, &QPushButton::clicked,
            q, [this]() { exportClicked(); });

    connect(Kleo::KeyCache::instance().get(), &Kleo::KeyCache::keysMayHaveChanged,
            q, [this]() { keysMayHaveChanged(); });
}

void CertificateDetailsWidget::Private::setupCommonProperties()
{
    // TODO: Enable once implemented
    HIDE_ROW(publishing)

    const bool hasSecret = key.hasSecret();
    const bool isOpenPGP = key.protocol() == GpgME::OpenPGP;
    // TODO: Enable once implemented
    const bool canRevokeUID = false; // isOpenPGP && hasSecret

    ui.changePassphraseBtn->setVisible(hasSecret);
    ui.genRevokeBtn->setVisible(isOpenPGP && hasSecret);
    ui.certifyBtn->setVisible(isOpenPGP && !hasSecret);
    ui.changeExpirationBtn->setVisible(isOpenPGP && hasSecret);
    ui.addUserIDBtn->setVisible(hasSecret && isOpenPGP);
    ui.webOfTrustBtn->setVisible(isOpenPGP);

    ui.hboxLayout_1->addStretch(1);

    ui.validFrom->setText(Kleo::Formatting::creationDateString(key));
    const QString expiry = Kleo::Formatting::expirationDateString(key);
    ui.expires->setText(expiry.isEmpty() ? i18nc("Expires", "never") : expiry);
    ui.type->setText(Kleo::Formatting::type(key));
    ui.fingerprint->setText(Formatting::prettyID(key.primaryFingerprint()));

    if (Kleo::Formatting::complianceMode().isEmpty()) {
        HIDE_ROW(compliance)
    } else {
        ui.complianceLbl->setText(Kleo::Formatting::complianceStringForKey(key));
    }

    ui.userIDTable->clear();

    QStringList headers = { i18n("Email"), i18n("Name"), i18n("Trust Level"), i18n("Tags") };
    if (canRevokeUID) {
        headers << QString();
    }
    ui.userIDTable->setColumnCount(headers.count());
    ui.userIDTable->setColumnWidth(0, 200);
    ui.userIDTable->setColumnWidth(1, 200);
    ui.userIDTable->setHeaderLabels(headers);

    const auto uids = key.userIDs();
    for (unsigned int i = 0; i < uids.size(); ++i) {
        const auto &uid = uids[i];
        auto item = new QTreeWidgetItem;
        const QString toolTip = tofuTooltipString(uid);
        item->setData(0, Qt::UserRole, QVariant::fromValue(uid));

        auto pMail = Kleo::Formatting::prettyEMail(uid);
        auto pName = Kleo::Formatting::prettyName(uid);
        if (!isOpenPGP && pMail.isEmpty() && !pName.isEmpty()) {
            // S/MIME UserIDs are sometimes split, with one userID
            // containing the name another the Mail, we merge these
            // UID's into a single item.

            if (i + 1 < uids.size()) {
                pMail = Kleo::Formatting::prettyEMail(uids[i + 1]);
                // skip next uid
                ++i;
            }
        }

        if (!isOpenPGP && pMail.isEmpty() && pName.isEmpty()) {
            // S/MIME certificates sometimes contain urls where both
            // name and mail is empty. In that case we print whatever
            // the uid is as name.
            //
            // Can be ugly like (3:uri24:http://ca.intevation.org), but
            // this is better then showing an empty entry.
            pName = QString::fromLatin1(uid.id());
        }

        item->setData(0, Qt::DisplayRole, pMail);
        item->setData(0, Qt::ToolTipRole, toolTip);
        item->setData(1, Qt::DisplayRole, pName);
        item->setData(1, Qt::ToolTipRole, toolTip);

        QIcon trustIcon;
        if (updateInProgress) {
           trustIcon = QIcon::fromTheme(QStringLiteral("emblem-question"));
           item->setData(2, Qt::DisplayRole, i18n("Updating..."));
        } else {
            switch (uid.validity()) {
            case GpgME::UserID::Unknown:
            case GpgME::UserID::Undefined:
                trustIcon = QIcon::fromTheme(QStringLiteral("emblem-question"));
                break;
            case GpgME::UserID::Never:
                trustIcon = QIcon::fromTheme(QStringLiteral("emblem-error"));
                break;
            case GpgME::UserID::Marginal:
                trustIcon = QIcon::fromTheme(QStringLiteral("emblem-warning"));
                break;
            case GpgME::UserID::Full:
            case GpgME::UserID::Ultimate:
                trustIcon = QIcon::fromTheme(QStringLiteral("emblem-success"));
                break;
            }
            item->setData(2, Qt::DisplayRole, Kleo::Formatting::validityShort(uid));
        }
        item->setData(2, Qt::DecorationRole, trustIcon);
        item->setData(2, Qt::ToolTipRole, toolTip);

        GpgME::Error err;
        QStringList tagList;
#ifdef GPGME_HAS_REMARKS
        for (const auto &tag: uid.remarks(Tags::tagKeys(), err)) {
            if (err) {
                qCWarning(KLEOPATRA_LOG) << "Getting remarks for user id" << uid.id() << "failed:" << err;
            }
            tagList << QString::fromStdString(tag);
        }
        qCDebug(KLEOPATRA_LOG) << "tagList:" << tagList;
#endif
        const auto tags = tagList.join(QStringLiteral("; "));
        item->setData(3, Qt::DisplayRole, tags);
        item->setData(3, Qt::ToolTipRole, toolTip);

        ui.userIDTable->addTopLevelItem(item);

        if (canRevokeUID) {
            auto button = new QPushButton;
            button->setIcon(QIcon::fromTheme(QStringLiteral("entry-delete")));
            button->setToolTip(i18n("Revoke this User ID"));
            button->setMaximumWidth(32);
            QObject::connect(button, &QPushButton::clicked,
                            q, [this, uid]() { revokeUID(uid); });
            ui.userIDTable->setItemWidget(item, 4, button);
        }
    }
    if (!Tags::tagsEnabled()) {
        ui.userIDTable->hideColumn(3);
    }
}

void CertificateDetailsWidget::Private::revokeUID(const GpgME::UserID &uid)
{
    Q_UNUSED(uid)
    qCWarning(KLEOPATRA_LOG) << "Revoking UserID is not implemented. How did you even get here?!?!";
}

void CertificateDetailsWidget::Private::changeExpiration()
{
    auto cmd = new Kleo::Commands::ChangeExpiryCommand(key);
    QObject::connect(cmd, &Kleo::Commands::ChangeExpiryCommand::finished,
                     q, [this]() {
                         ui.changeExpirationBtn->setEnabled(true);
                     });
    ui.changeExpirationBtn->setEnabled(false);
    cmd->start();
}

void CertificateDetailsWidget::Private::changePassphrase()
{
    auto cmd = new Kleo::Commands::ChangePassphraseCommand(key);
    QObject::connect(cmd, &Kleo::Commands::ChangePassphraseCommand::finished,
                     q, [this]() {
                         ui.changePassphraseBtn->setEnabled(true);
                     });
    ui.changePassphraseBtn->setEnabled(false);
    cmd->start();
}

void CertificateDetailsWidget::Private::genRevokeCert()
{
    auto cmd = new Kleo::Commands::GenRevokeCommand(key);
    QObject::connect(cmd, &Kleo::Commands::GenRevokeCommand::finished,
                     q, [this]() {
                         ui.genRevokeBtn->setEnabled(true);
                     });
    ui.genRevokeBtn->setEnabled(false);
    cmd->start();
}

void CertificateDetailsWidget::Private::certifyClicked()
{
    auto cmd = new Kleo::Commands::CertifyCertificateCommand(key);
    QObject::connect(cmd, &Kleo::Commands::CertifyCertificateCommand::finished,
                     q, [this]() {
                         ui.certifyBtn->setEnabled(true);
                     });
    ui.certifyBtn->setEnabled(false);
    cmd->start();
}

void CertificateDetailsWidget::Private::webOfTrustClicked()
{
    QScopedPointer<WebOfTrustDialog> dlg(new WebOfTrustDialog(q));
    dlg->setKey(key);
    dlg->exec();
}

void CertificateDetailsWidget::Private::exportClicked()
{
    QScopedPointer<ExportDialog> dlg(new ExportDialog(q));
    dlg->setKey(key);
    dlg->exec();
}

void CertificateDetailsWidget::Private::addUserID()
{
    auto cmd = new Kleo::Commands::AddUserIDCommand(key);
    QObject::connect(cmd, &Kleo::Commands::AddUserIDCommand::finished,
                     q, [this]() {
                         ui.addUserIDBtn->setEnabled(true);
                         key.update();
                         q->setKey(key);
                     });
    ui.addUserIDBtn->setEnabled(false);
    cmd->start();
}

void CertificateDetailsWidget::Private::keysMayHaveChanged()
{
    auto newKey = Kleo::KeyCache::instance()->findByFingerprint(key.primaryFingerprint());
    if (!newKey.isNull()) {
        setUpdatedKey(newKey);
    }
}

void CertificateDetailsWidget::Private::showTrustChainDialog()
{
    QScopedPointer<TrustChainDialog> dlg(new TrustChainDialog(q));
    dlg->setKey(key);
    dlg->exec();
}

void CertificateDetailsWidget::Private::publishCertificate()
{
    qCWarning(KLEOPATRA_LOG) << "publishCertificateis not implemented.";
    //TODO
}

void CertificateDetailsWidget::Private::userIDTableContextMenuRequested(const QPoint &p)
{
    auto item = ui.userIDTable->itemAt(p);
    if (!item) {
        return;
    }

    const auto userID = item->data(0, Qt::UserRole).value<GpgME::UserID>();

    auto menu = new QMenu(q);
    menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-sign")),
                    i18n("Certify..."),
                    q, [this, userID]() {
        auto cmd = new Kleo::Commands::CertifyCertificateCommand(userID);
        ui.userIDTable->setEnabled(false);
        connect(cmd, &Kleo::Commands::CertifyCertificateCommand::finished,
                q, [this]() {
            ui.userIDTable->setEnabled(true);
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
            ui.userIDTable->setEnabled(false);
            connect(cmd, &Kleo::Commands::RevokeCertificationCommand::finished,
                    q, [this]() {
                ui.userIDTable->setEnabled(true);
                // Trigger an update when done
                q->setKey(key);
            });
            cmd->start();
        });
    }
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(ui.userIDTable->viewport()->mapToGlobal(p));
}

void CertificateDetailsWidget::Private::showMoreDetails()
{
    ui.moreDetailsBtn->setEnabled(false);
    if (key.protocol() == GpgME::CMS) {
        auto cmd = new Kleo::Commands::DumpCertificateCommand(key);
        connect(cmd, &Kleo::Commands::DumpCertificateCommand::finished,
                q, [this]() {
                    ui.moreDetailsBtn->setEnabled(true);
                });
        cmd->setUseDialog(true);
        cmd->start();
    } else {
        QScopedPointer<SubKeysDialog> dlg(new SubKeysDialog(q));
        dlg->setKey(key);
        dlg->exec();
        ui.moreDetailsBtn->setEnabled(true);
    }
}

QString CertificateDetailsWidget::Private::tofuTooltipString(const GpgME::UserID &uid) const
{
    const auto tofu = uid.tofuInfo();
    if (tofu.isNull()) {
        return QString();
    }

    QString html = QStringLiteral("<table border=\"0\" cell-padding=\"5\">");
    const auto appendRow = [&html](const QString &lbl, const QString &val) {
        html += QStringLiteral("<tr>"
                               "<th style=\"text-align: right; padding-right: 5px; white-space: nowrap;\">%1:</th>"
                               "<td style=\"white-space: nowrap;\">%2</td>"
                               "</tr>")
                    .arg(lbl, val);
    };
    const auto appendHeader = [this, &html](const QString &hdr) {
        html += QStringLiteral("<tr><th colspan=\"2\" style=\"background-color: %1; color: %2\">%3</th></tr>")
                    .arg(q->palette().highlight().color().name(),
                         q->palette().highlightedText().color().name(),
                         hdr);
    };
    const auto dateTime = [](long ts) {
        QLocale l;
        return ts == 0 ? i18n("never") : l.toString(QDateTime::fromSecsSinceEpoch(ts), QLocale::ShortFormat);
    };
    appendHeader(i18n("Signing"));
    appendRow(i18n("First message"), dateTime(tofu.signFirst()));
    appendRow(i18n("Last message"), dateTime(tofu.signLast()));
    appendRow(i18n("Message count"), QString::number(tofu.signCount()));
    appendHeader(i18n("Encryption"));
    appendRow(i18n("First message"), dateTime(tofu.encrFirst()));
    appendRow(i18n("Last message"), dateTime(tofu.encrLast()));
    appendRow(i18n("Message count"), QString::number(tofu.encrCount()));

    html += QStringLiteral("</table>");
    // Make sure the tooltip string is different for each UserID, even if the
    // data are the same, otherwise the tooltip is not updated and moved when
    // user moves mouse from one row to another.
    html += QStringLiteral("<!-- %1 //-->").arg(QString::fromUtf8(uid.id()));
    return html;
}

#ifdef GPGMEPP_SUPPORTS_TRUST_SIGNATURES
namespace
{
auto isGood(const GpgME::UserID::Signature &signature)
{
    return signature.status() == GpgME::UserID::Signature::NoError &&
           !signature.isInvalid() &&
           0x10 <= signature.certClass() && signature.certClass() <= 0x13;
}

auto accumulateTrustDomains(const std::vector<GpgME::UserID::Signature> &signatures)
{
    return std::accumulate(
        std::begin(signatures), std::end(signatures),
        std::set<QString>(),
        [] (auto domains, const auto &signature) {
            if (isGood(signature) && signature.isTrustSignature()) {
                domains.insert(Formatting::trustSignatureDomain(signature));
            }
            return domains;
        }
    );
}

auto accumulateTrustDomains(const std::vector<GpgME::UserID> &userIds)
{
    return std::accumulate(
        std::begin(userIds), std::end(userIds),
        std::set<QString>(),
        [] (auto domains, const auto &userID) {
            const auto newDomains = accumulateTrustDomains(userID.signatures());
            std::copy(std::begin(newDomains), std::end(newDomains), std::inserter(domains, std::end(domains)));
            return domains;
        }
    );
}
}
#endif

void CertificateDetailsWidget::Private::setupPGPProperties()
{
    HIDE_ROW(smimeOwner)
    HIDE_ROW(smimeIssuer)
    ui.smimeRelatedAddresses->setVisible(false);
    ui.trustChainDetailsBtn->setVisible(false);

    ui.userIDTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui.userIDTable, &QAbstractItemView::customContextMenuRequested,
            q, [this](const QPoint &p) { userIDTableContextMenuRequested(p); });

#ifdef GPGMEPP_SUPPORTS_TRUST_SIGNATURES
    const auto trustDomains = accumulateTrustDomains(key.userIDs());
    if (trustDomains.empty()) {
        HIDE_ROW(trustedIntroducer)
    } else {
        SHOW_ROW(trustedIntroducer)
        ui.trustedIntroducer->setText(QStringList(std::begin(trustDomains), std::end(trustDomains)).join(u", "));
    }
#else
    HIDE_ROW(trustedIntroducer)
#endif
}

static QString formatDNToolTip(const Kleo::DN &dn)
{
    QString html = QStringLiteral("<table border=\"0\" cell-spacing=15>");

    const auto appendRow = [&html, dn](const QString &lbl, const QString &attr) {
                                const QString val = dn[attr];
                                if (!val.isEmpty()) {
                                    html += QStringLiteral(
                                            "<tr><th style=\"text-align: left; white-space: nowrap\">%1:</th>"
                                                "<td style=\"white-space: nowrap\">%2</td>"
                                            "</tr>").arg(lbl, val);
                                }
                            };
    appendRow(i18n("Common Name"), QStringLiteral("CN"));
    appendRow(i18n("Organization"), QStringLiteral("O"));
    appendRow(i18n("Street"), QStringLiteral("STREET"));
    appendRow(i18n("City"),  QStringLiteral("L"));
    appendRow(i18n("State"), QStringLiteral("ST"));
    appendRow(i18n("Country"), QStringLiteral("C"));
    html += QStringLiteral("</table>");

    return html;
}

void CertificateDetailsWidget::Private::setupSMIMEProperties()
{
    HIDE_ROW(publishing)
    HIDE_ROW(trustedIntroducer)

    const auto ownerId = key.userID(0);
    const Kleo::DN dn(ownerId.id());
    const QString cn = dn[QStringLiteral("CN")];
    const QString o = dn[QStringLiteral("O")];
    const QString dnEmail = dn[QStringLiteral("EMAIL")];
    const QString name = cn.isEmpty() ? dnEmail : cn;

    QString owner;
    if (name.isEmpty()) {
        owner = dn.dn();
    } else if (o.isEmpty()) {
        owner = name;
    } else {
        owner = i18nc("<name> of <company>", "%1 of %2", name, o);
    }
    ui.smimeOwner->setText(owner);
    ui.smimeOwner->setTextInteractionFlags(Qt::TextBrowserInteraction);

    const Kleo::DN issuerDN(key.issuerName());
    const QString issuerCN = issuerDN[QStringLiteral("CN")];
    const QString issuer = issuerCN.isEmpty() ? QString::fromUtf8(key.issuerName()) : issuerCN;
    ui.smimeIssuer->setText(QStringLiteral("<a href=\"#issuerDetails\">%1</a>").arg(issuer));

    ui.smimeIssuer->setToolTip(formatDNToolTip(issuerDN));

    ui.smimeOwner->setToolTip(formatDNToolTip(dn));

}

void CertificateDetailsWidget::Private::smimeLinkActivated(const QString &link)
{
    if (link == QLatin1String("#issuerDetails")) {
        const auto parentKey = KeyCache::instance()->findIssuers(key, KeyCache::NoOption);

        if (!parentKey.size()) {
            return;
        }
        auto cmd = new Kleo::Commands::DetailsCommand(parentKey[0], nullptr);
        cmd->setParentWidget(q);
        cmd->start();
        return;
    }
    qCWarning(KLEOPATRA_LOG) << "Unknown link activated:" << link;
}

CertificateDetailsWidget::CertificateDetailsWidget(QWidget *parent)
    : QWidget{parent}
    , d{std::make_unique<Private>(this)}
{
}

CertificateDetailsWidget::~CertificateDetailsWidget() = default;

void CertificateDetailsWidget::Private::keyListDone(const GpgME::KeyListResult &,
                                                    const std::vector<GpgME::Key> &keys,
                                                    const QString &,
                                                    const GpgME::Error &)
{
    updateInProgress = false;
    if (keys.size() != 1) {
        qCWarning(KLEOPATRA_LOG) << "Invalid keylist result in update.";
        return;
    }
    // As we listen for keysmayhavechanged we get the update
    // after updating the keycache.
    KeyCache::mutableInstance()->insert(keys);
}

void CertificateDetailsWidget::Private::setUpdatedKey(const GpgME::Key &k)
{
    key = k;

    setupCommonProperties();
    if (key.protocol() == GpgME::OpenPGP) {
        setupPGPProperties();
    } else {
        setupSMIMEProperties();
    }
}

void CertificateDetailsWidget::setKey(const GpgME::Key &key)
{
    if (key.protocol() == GpgME::CMS) {
        // For everything but S/MIME this should be quick
        // and we don't need to show another status.
        d->updateInProgress = true;
    }
    d->setUpdatedKey(key);

    // Run a keylistjob with full details (TOFU / Validate)
    QGpgME::KeyListJob *job = key.protocol() == GpgME::OpenPGP ? QGpgME::openpgp()->keyListJob(false, true, true) :
                                                                 QGpgME::smime()->keyListJob(false, true, true);

    auto ctx = QGpgME::Job::context(job);
    ctx->addKeyListMode(GpgME::WithTofu);
    ctx->addKeyListMode(GpgME::SignatureNotations);
#ifdef GPGME_HAS_WITH_SECRET
    if (key.hasSecret()) {
        ctx->addKeyListMode(GpgME::WithSecret);
    }
#endif

    // Windows QGpgME new style connect problem makes this necessary.
    connect(job, SIGNAL(result(GpgME::KeyListResult,std::vector<GpgME::Key>,QString,GpgME::Error)),
            this, SLOT(keyListDone(GpgME::KeyListResult,std::vector<GpgME::Key>,QString,GpgME::Error)));

#ifdef GPGME_HAS_WITH_SECRET
    job->start(QStringList() << QLatin1String(key.primaryFingerprint()));
#else
    job->start(QStringList() << QLatin1String(key.primaryFingerprint()), key.hasSecret());
#endif
}

GpgME::Key CertificateDetailsWidget::key() const
{
    return d->key;
}

#include "moc_certificatedetailswidget.cpp"
