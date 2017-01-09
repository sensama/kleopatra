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

#include "certificatedetailswidget.h"
#include "ui_certificatedetailswidget.h"
#include "kleopatra_debug.h"
#include "trustchainwidget.h"
#include "subkeyswidget.h"

#include "commands/changepassphrasecommand.h"
#include "commands/changeexpirycommand.h"
#include "commands/certifycertificatecommand.h"
#include "commands/adduseridcommand.h"
#include "commands/dumpcertificatecommand.h"

#include <libkleo/formatting.h>
#include <libkleo/dn.h>
#include <libkleo/keycache.h>

#include <gpgme++/key.h>
#include <gpgme++/tofuinfo.h>

#include <QStandardItemModel>
#include <QToolTip>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QMenu>
#include <KConfigGroup>
#include <KSharedConfig>

#define HIDE_ROW(row) \
    ui.row->setVisible(false); \
    ui.row##Lbl->setVisible(false);

Q_DECLARE_METATYPE(GpgME::UserID);

class CertificateDetailsWidget::Private
{
public:
    Private(CertificateDetailsWidget *parent)
        : q(parent)
    {}

    void setupCommonProperties();
    void setupPGPProperties();
    void setupSMIMEProperties();

    void revokeUID(const GpgME::UserID &uid);
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

    Ui::CertificateDetailsWidget ui;
    GpgME::Key key;
private:
    CertificateDetailsWidget *q;
};

void CertificateDetailsWidget::Private::setupCommonProperties()
{
    // TODO: Enable once implemented
    HIDE_ROW(publishing)

    const bool hasSecret = key.hasSecret();
    const bool isOpenPGP = key.protocol() == GpgME::OpenPGP;
    // TODO: Enable once implemented
    const bool canRevokeUID = false; // isOpenPGP && hasSecret

    ui.changePassphraseBtn->setVisible(hasSecret);
    ui.changeExpirationBtn->setVisible(isOpenPGP && hasSecret);
    ui.addUserIDBtn->setVisible(hasSecret);

    ui.validFrom->setText(Kleo::Formatting::creationDateString(key));
    const QString expiry = Kleo::Formatting::expirationDateString(key);
    ui.expires->setText(expiry.isEmpty() ? i18nc("Expires", "never") : expiry);
    ui.type->setText(Kleo::Formatting::type(key));
    ui.fingerprint->setText(QString::fromLatin1(key.primaryFingerprint()));

    ui.userIDTable->clear();

    QStringList headers = { i18n("Email"), i18n("Name") };
    if (isOpenPGP) {
        headers << i18n("Trust Level");
        if (canRevokeUID) {
            headers << QString();
        }
    }
    ui.userIDTable->setColumnCount(headers.count());
    ui.userIDTable->setColumnWidth(0, 200);
    ui.userIDTable->setColumnWidth(1, 200);
    ui.userIDTable->setHeaderLabels(headers);

    const auto uids = key.userIDs();
    for (const auto &uid : uids) {
        auto item = new QTreeWidgetItem;
        const QString toolTip = tofuTooltipString(uid);
        item->setData(0, Qt::UserRole, QVariant::fromValue(uid));
        item->setData(0, Qt::DisplayRole, Kleo::Formatting::prettyEMail(uid));
        item->setData(0, Qt::ToolTipRole, toolTip);
        item->setData(1, Qt::DisplayRole, Kleo::Formatting::prettyName(uid));
        item->setData(1, Qt::ToolTipRole, toolTip);
        if (isOpenPGP) {
            QIcon trustIcon;
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
            item->setData(2, Qt::DecorationRole, trustIcon);
            item->setData(2, Qt::DisplayRole, Kleo::Formatting::validityShort(uid));
            item->setData(2, Qt::ToolTipRole, toolTip);

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
        } else {
            ui.userIDTable->addTopLevelItem(item);
        }
    }
}

void CertificateDetailsWidget::Private::revokeUID(const GpgME::UserID &uid)
{
    Q_UNUSED(uid);
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
                     q, [this, cmd]() {
                         ui.changePassphraseBtn->setEnabled(true);
                     });
    ui.changePassphraseBtn->setEnabled(false);
    cmd->start();
}

void CertificateDetailsWidget::Private::addUserID()
{
    auto cmd = new Kleo::Commands::AddUserIDCommand(key);
    QObject::connect(cmd, &Kleo::Commands::AddUserIDCommand::finished,
                     q, [this, cmd]() {
                         ui.addUserIDBtn->setEnabled(true);
                     });
    ui.addUserIDBtn->setEnabled(false);
    cmd->start();
}

void CertificateDetailsWidget::Private::keysMayHaveChanged()
{
    auto newKey = Kleo::KeyCache::instance()->findByFingerprint(key.primaryFingerprint());
    if (!newKey.isNull()) {
        q->setKey(newKey);
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

    QMenu *menu = new QMenu(q);
    menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-sign")),
                    i18n("Certify ..."),
                    q, [this, userID]() {
        auto cmd = new Kleo::Commands::CertifyCertificateCommand(userID);
        ui.userIDTable->setEnabled(false);
        connect(cmd, &Kleo::Commands::CertifyCertificateCommand::finished,
                q, [this]() { ui.userIDTable->setEnabled(true); });
        cmd->start();
    });
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
        return ts == 0 ? i18n("never") : QDateTime::fromTime_t(ts).toString(Qt::SystemLocaleShortDate);
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


void CertificateDetailsWidget::Private::setupPGPProperties()
{
    HIDE_ROW(smimeOwner)
    HIDE_ROW(smimeIssuer)
    ui.smimeRelatedAddresses->setVisible(false);
    ui.trustChainDetailsBtn->setVisible(false);

    ui.userIDTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui.userIDTable, &QAbstractItemView::customContextMenuRequested,
            q, [this](const QPoint &p) { userIDTableContextMenuRequested(p); });
}

void CertificateDetailsWidget::Private::setupSMIMEProperties()
{
    HIDE_ROW(publishing)

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
    ui.smimeOwner->setText(QStringLiteral("<a href=\"#ownerDetails\">%1</a>").arg(owner));

    const Kleo::DN issuerDN(key.issuerName());
    const QString issuerCN = issuerDN[QStringLiteral("CN")];
    const QString issuer = issuerCN.isEmpty() ? key.issuerName() : issuerCN;
    ui.smimeIssuer->setText(QStringLiteral("<a href=\"#issuerDetails\">%1</a>").arg(issuer));
}

void CertificateDetailsWidget::Private::smimeLinkActivated(const QString &link)
{
    Kleo::DN dn;
    QWidget *w;
    if (link == QLatin1String("#ownerDetails")) {
        dn = Kleo::DN(key.userID(0).id());
        w = ui.smimeOwner;
    } else if (link == QLatin1String("#issuerDetails")) {
        dn = Kleo::DN(key.issuerName());
        w = ui.smimeIssuer;
    } else {
        Q_UNREACHABLE();
        return;
    }

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

    QToolTip::showText(QCursor::pos(), html, w);
}



CertificateDetailsWidget::CertificateDetailsWidget(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{
    d->ui.setupUi(this);

    connect(d->ui.addUserIDBtn, &QPushButton::clicked,
            this, [this]() { d->addUserID(); });
    connect(d->ui.changePassphraseBtn, &QPushButton::clicked,
            this, [this]() { d->changePassphrase(); });
    connect(d->ui.changeExpirationBtn, &QPushButton::clicked,
            this, [this]() { d->changeExpiration(); });
    connect(d->ui.smimeOwner, &QLabel::linkActivated,
            this, [this](const QString &link) { d->smimeLinkActivated(link); });
    connect(d->ui.smimeIssuer, &QLabel::linkActivated,
            this, [this](const QString &link) { d->smimeLinkActivated(link); });
    connect(d->ui.trustChainDetailsBtn, &QPushButton::pressed,
            this, [this]() { d->showTrustChainDialog(); });
    connect(d->ui.moreDetailsBtn, &QPushButton::pressed,
            this, [this]() { d->showMoreDetails(); });
    connect(d->ui.publishing, &QPushButton::pressed,
            this, [this]() { d->publishCertificate(); });

    connect(Kleo::KeyCache::instance().get(), &Kleo::KeyCache::keysMayHaveChanged,
            this, [this]() { d->keysMayHaveChanged(); });
}

CertificateDetailsWidget::~CertificateDetailsWidget()
{
}

void CertificateDetailsWidget::setKey(const GpgME::Key &key)
{
    d->key = key;
    d->key.update(); // Fetch TOFU info (TODO: could be blocking, use async?)

    d->setupCommonProperties();
    if (key.protocol() == GpgME::OpenPGP) {
        d->setupPGPProperties();
    } else {
        d->setupSMIMEProperties();
    }
}

GpgME::Key CertificateDetailsWidget::key() const
{
    return d->key;
}

CertificateDetailsDialog::CertificateDetailsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Certificate Details"));
    auto l = new QVBoxLayout(this);
    l->addWidget(new CertificateDetailsWidget(this));

    auto bbox = new QDialogButtonBox(this);
    auto btn = bbox->addButton(QDialogButtonBox::Close);
    connect(btn, &QPushButton::pressed, this, &QDialog::accept);
    l->addWidget(bbox);
    readConfig();
}

CertificateDetailsDialog::~CertificateDetailsDialog()
{
    writeConfig();
}

void CertificateDetailsDialog::readConfig()
{
    KConfigGroup dialog(KSharedConfig::openConfig(), "CertificateDetailsDialog");
    const QSize size = dialog.readEntry("Size", QSize(730, 280));
    if (size.isValid()) {
        resize(size);
    }
}

void CertificateDetailsDialog::writeConfig()
{
    KConfigGroup dialog(KSharedConfig::openConfig(), "CertificateDetailsDialog");
    dialog.writeEntry("Size", size());
    dialog.sync();
}

void CertificateDetailsDialog::setKey(const GpgME::Key &key)
{
    auto w = findChild<CertificateDetailsWidget*>();
    Q_ASSERT(w);
    w->setKey(key);
}

GpgME::Key CertificateDetailsDialog::key() const
{
    auto w = findChild<CertificateDetailsWidget*>();
    Q_ASSERT(w);
    return w->key();
}
