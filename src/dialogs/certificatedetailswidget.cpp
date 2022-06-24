/*
    dialogs/certificatedetailswidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2017 Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>
    SPDX-FileCopyrightText: 2022 Felix Tiede

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
#ifdef MAILAKONADI_ENABLED
#include "commands/exportopenpgpcerttoprovidercommand.h"
#endif // MAILAKONADI_ENABLED
#include "commands/refreshcertificatecommand.h"
#include "commands/revokecertificationcommand.h"
#include "commands/revokeuseridcommand.h"
#include "commands/adduseridcommand.h"
#include "commands/genrevokecommand.h"
#include "commands/detailscommand.h"
#include "commands/dumpcertificatecommand.h"

#include "utils/keys.h"
#include "utils/tags.h"

#include <Libkleo/Formatting>
#include <Libkleo/Dn>
#include <Libkleo/KeyCache>
#include <Libkleo/GnuPG>

#include <KLocalizedString>
#include <KMessageBox>

#include <gpgme++/context.h>
#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>
#include <gpgme++/tofuinfo.h>

#include <QGpgME/Debug>
#include <QGpgME/Protocol>
#include <QGpgME/KeyListJob>

#include <QClipboard>
#include <QDateTime>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QStringBuilder>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <set>

Q_DECLARE_METATYPE(GpgME::UserID)

using namespace Kleo;

static const char *accessibleNameProperty = "_kleo_accessibleName";

class InfoField {
public:
    InfoField(const QString &label, QWidget *parent);

    void setValue(const QString &value, const QString &accessibleValue = {});
    void setAction(const QAction *action);
    void setToolTip(const QString &toolTip);
    void setVisible(bool visible);

    QLabel *label() const { return mLabel; }
    QLayout *layout() const { return mLayout; }

private:
    void onActionChanged();

    QLabel *mLabel = nullptr;
    QHBoxLayout *mLayout = nullptr;
    QLabel *mValue = nullptr;
    QPushButton *mButton = nullptr;
    const QAction *mAction = nullptr;
};

InfoField::InfoField(const QString &label, QWidget *parent)
    : mLabel{new QLabel{label, parent}}
    , mLayout{new QHBoxLayout}
    , mValue{new QLabel{parent}}
    , mButton{new QPushButton{parent}}
{
    mLabel->setBuddy(mValue);
    mLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mValue->setFocusPolicy(Qt::TabFocus);
    mLayout->addWidget(mValue);
    mButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    mButton->setVisible(false);
    mLayout->addWidget(mButton);
    mLayout->addStretch();
}

void InfoField::setValue(const QString &value, const QString &accessibleValue)
{
    mValue->setText(value);
    mValue->setAccessibleName(accessibleValue);
}

void InfoField::setAction(const QAction *action)
{
    if (action == mAction) {
        return;
    }
    if (mAction) {
        QObject::disconnect(mButton, {}, mAction, {});
        QObject::disconnect(mAction, {}, mButton, {});
    }
    mAction = action;
    if (mAction) {
        QObject::connect(mButton, &QPushButton::clicked, action, &QAction::trigger);
        QObject::connect(mAction, &QAction::changed, mButton, [this]() {
            onActionChanged();
        });
        onActionChanged();
        mButton->setAccessibleName(mAction->property(accessibleNameProperty).toString());
        mButton->setVisible(true);
    } else {
        mButton->setVisible(false);
        mButton->setText({});
        mButton->setIcon({});
    }
}

void InfoField::setToolTip(const QString &toolTip)
{
    mValue->setToolTip(toolTip);
}

void InfoField::setVisible(bool visible)
{
    mLabel->setVisible(visible);
    mValue->setVisible(visible);
    mButton->setVisible(visible && mAction);
}

void InfoField::onActionChanged()
{
    if (!mAction) {
        return;
    }
    if (mAction->text() != mButton->text()) {
        mButton->setText(mAction->text());
    }
    mButton->setIcon(mAction->icon());
    if (mAction->toolTip() != mButton->toolTip()) {
        mButton->setToolTip(mAction->toolTip());
    }
    if (mAction->isEnabled() != mButton->isEnabled()) {
        mButton->setEnabled(mAction->isEnabled());
    }
}

class CertificateDetailsWidget::Private
{
public:
    Private(CertificateDetailsWidget *qq);

    void setupCommonProperties();
    void setupPGPProperties();
    void setupSMIMEProperties();

    void revokeUserID(const GpgME::UserID &uid);
    void genRevokeCert();
    void refreshCertificate();
    void certifyClicked();
    void webOfTrustClicked();
    void exportClicked();
    void addUserID();
    void changePassphrase();
    void changeExpiration();
    void keysMayHaveChanged();
    void showTrustChainDialog();
    void showMoreDetails();
    void userIDTableContextMenuRequested(const QPoint &p);

    QString tofuTooltipString(const GpgME::UserID &uid) const;

    void showIssuerCertificate();

    void updateKey();
    void setUpdatedKey(const GpgME::Key &key);
    void keyListDone(const GpgME::KeyListResult &,
                     const std::vector<GpgME::Key> &, const QString &,
                     const GpgME::Error &);
    void copyFingerprintToClipboard();

private:
    CertificateDetailsWidget *const q;

public:
    GpgME::Key key;
    bool updateInProgress = false;

private:
    struct UI {
        QLabel *label = nullptr;
        InfoField *smimeOwnerField = nullptr;
        QLabel *smimeRelatedAddresses = nullptr;
        QTreeWidget *userIDTable = nullptr;
        QGroupBox *groupBox = nullptr;
        InfoField *validFromField = nullptr;
        InfoField *expiresField = nullptr;
        QAction *changeExpirationAction = nullptr;
        InfoField *typeField = nullptr;
        InfoField *fingerprintField = nullptr;
        QAction *copyFingerprintAction = nullptr;
        InfoField *smimeIssuerField = nullptr;
        QAction *showIssuerCertificateAction = nullptr;
        InfoField *complianceField = nullptr;
        InfoField *trustedIntroducerField = nullptr;
        QPushButton *addUserIDBtn = nullptr;
        QPushButton *changePassphraseBtn = nullptr;
        QPushButton *trustChainDetailsBtn = nullptr;
        QPushButton *genRevokeBtn = nullptr;
        QPushButton *refreshBtn = nullptr;
        QPushButton *certifyBtn = nullptr;
        QPushButton *moreDetailsBtn = nullptr;
        QPushButton *exportBtn = nullptr;
        QPushButton *webOfTrustBtn = nullptr;

        void setupUi(QWidget *parent)
        {
            auto mainLayout = new QVBoxLayout{parent};

            label = new QLabel(i18n("You can use this certificate to secure communication with the following email addresses:"), parent);
            label->setWordWrap(true);

            mainLayout->addWidget(label);

            {
            auto gridLayout_2 = new QGridLayout;
            gridLayout_2->setColumnStretch(1, 1);

            int row = 0;
            smimeOwnerField = new InfoField{i18n("Owner:"), parent};
            gridLayout_2->addWidget(smimeOwnerField->label(), row, 0, 1, 1);
            gridLayout_2->addLayout(smimeOwnerField->layout(), row, 1, 1, 1);

            row++;
            smimeRelatedAddresses = new QLabel(i18n("Related addresses:"), parent);
            QFont font;
            font.setBold(true);
            font.setWeight(75);
            smimeRelatedAddresses->setFont(font);

            gridLayout_2->addWidget(smimeRelatedAddresses, row, 0, 1, 1);

            mainLayout->addLayout(gridLayout_2);
            }

            userIDTable = new QTreeWidget(parent);
            QTreeWidgetItem *__qtreewidgetitem = new QTreeWidgetItem();
            __qtreewidgetitem->setText(0, QString::fromUtf8("1"));
            userIDTable->setHeaderItem(__qtreewidgetitem);
            userIDTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
            userIDTable->setSelectionMode(QAbstractItemView::SingleSelection);
            userIDTable->setRootIsDecorated(false);
            userIDTable->setUniformRowHeights(true);
            userIDTable->setAllColumnsShowFocus(true);

            mainLayout->addWidget(userIDTable);

            {
            auto hboxLayout_1 = new QHBoxLayout;

            addUserIDBtn = new QPushButton(i18nc("@action:button", "Add User ID"), parent);
            hboxLayout_1->addWidget(addUserIDBtn);

            changePassphraseBtn = new QPushButton(i18nc("@action:button", "Change Passphrase"), parent);
            hboxLayout_1->addWidget(changePassphraseBtn);

            trustChainDetailsBtn = new QPushButton(i18nc("@action:button", "Trust Chain Details..."), parent);
            hboxLayout_1->addWidget(trustChainDetailsBtn);

            genRevokeBtn = new QPushButton(i18nc("@action:button", "Generate Revocation Certificate"), parent);
            genRevokeBtn->setToolTip(u"<html>" %
                                     i18n("A revocation certificate is a file that serves as a \"kill switch\" to publicly "
                                          "declare that a key shall not anymore be used.  It is not possible "
                                          "to retract such a revocation certificate once it has been published.") %
                                     u"</html>");
            hboxLayout_1->addWidget(genRevokeBtn);

            refreshBtn = new QPushButton{i18nc("@action:button", "Update"), parent};
#ifndef QGPGME_SUPPORTS_KEY_REFRESH
            refreshBtn->setVisible(false);
#endif
            hboxLayout_1->addWidget(refreshBtn);

            certifyBtn = new QPushButton(i18nc("@action:button", "Certify"), parent);
            hboxLayout_1->addWidget(certifyBtn);

            hboxLayout_1->addStretch(1);

            mainLayout->addLayout(hboxLayout_1);
            }

            groupBox = new QGroupBox(i18n("Certificate Details"), parent);
            groupBox->setFlat(false);
            {
                auto gridLayout = new QGridLayout(groupBox);
                gridLayout->setColumnStretch(1, 1);

                int boxRow = 0;
                validFromField = new InfoField{i18n("Valid from:"), groupBox};
                gridLayout->addWidget(validFromField->label(), boxRow, 0, 1, 1);
                gridLayout->addLayout(validFromField->layout(), boxRow, 1, 1, 1);

                boxRow++;
                expiresField = new InfoField{i18n("Expires:"), groupBox};
                changeExpirationAction = new QAction{parent};
                changeExpirationAction->setIcon(QIcon::fromTheme(QStringLiteral("editor")));
                changeExpirationAction->setToolTip(i18nc("@info:tooltip", "Change the expiration date"));
                changeExpirationAction->setProperty(accessibleNameProperty, i18nc("@action:button", "Change expiration"));
                expiresField->setAction(changeExpirationAction);
                gridLayout->addWidget(expiresField->label(), boxRow, 0, 1, 1);
                gridLayout->addLayout(expiresField->layout(), boxRow, 1, 1, 1);

                boxRow++;
                typeField = new InfoField{i18n("Type:"), groupBox};
                gridLayout->addWidget(typeField->label(), boxRow, 0, 1, 1);
                gridLayout->addLayout(typeField->layout(), boxRow, 1, 1, 1);

                boxRow++;
                fingerprintField = new InfoField{i18n("Fingerprint:"), groupBox};
                if (QGuiApplication::clipboard()) {
                    copyFingerprintAction = new QAction{parent};
                    copyFingerprintAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
                    copyFingerprintAction->setToolTip(i18nc("@info:tooltip", "Copy the fingerprint to the clipboard"));
                    copyFingerprintAction->setProperty(accessibleNameProperty, i18nc("@action:button", "Copy fingerprint"));
                    fingerprintField->setAction(copyFingerprintAction);
                }
                gridLayout->addWidget(fingerprintField->label(), boxRow, 0, 1, 1);
                gridLayout->addLayout(fingerprintField->layout(), boxRow, 1, 1, 1);

                boxRow++;
                smimeIssuerField = new InfoField{i18n("Issuer:"), groupBox};
                showIssuerCertificateAction = new QAction{parent};
                showIssuerCertificateAction->setIcon(QIcon::fromTheme(QStringLiteral("dialog-information")));
                showIssuerCertificateAction->setToolTip(i18nc("@info:tooltip", "Show the issuer certificate"));
                showIssuerCertificateAction->setProperty(accessibleNameProperty, i18nc("@action:button", "Show certificate"));
                smimeIssuerField->setAction(showIssuerCertificateAction);
                gridLayout->addWidget(smimeIssuerField->label(), boxRow, 0, 1, 1);
                gridLayout->addLayout(smimeIssuerField->layout(), boxRow, 1, 1, 1);

                boxRow++;
                complianceField = new InfoField{i18n("Compliance:"), groupBox};
                gridLayout->addWidget(complianceField->label(), boxRow, 0, 1, 1);
                gridLayout->addLayout(complianceField->layout(), boxRow, 1, 1, 1);

                boxRow++;
                trustedIntroducerField = new InfoField{i18n("Trusted introducer for:"), groupBox};
                gridLayout->addWidget(trustedIntroducerField->label(), boxRow, 0, 1, 1);
                trustedIntroducerField->setToolTip(i18n("See certifications for details."));
                gridLayout->addLayout(trustedIntroducerField->layout(), boxRow, 1, 1, 1);

                boxRow++;
                auto horizontalLayout = new QHBoxLayout;

                moreDetailsBtn = new QPushButton(i18nc("@action:button", "More Details..."), groupBox);
                horizontalLayout->addWidget(moreDetailsBtn);

                exportBtn = new QPushButton(i18nc("@action:button", "Export..."), groupBox);
                horizontalLayout->addWidget(exportBtn);

                webOfTrustBtn = new QPushButton(i18nc("@action:button", "Certifications..."), groupBox);
                horizontalLayout->addWidget(webOfTrustBtn);

                horizontalLayout->addStretch(1);

                gridLayout->addLayout(horizontalLayout, boxRow, 0, 1, 2);
            }
            mainLayout->addWidget(groupBox);
        }
    } ui;
};

CertificateDetailsWidget::Private::Private(CertificateDetailsWidget *qq)
    : q{qq}
{
    ui.setupUi(q);

    connect(ui.userIDTable, &QAbstractItemView::customContextMenuRequested,
            q, [this](const QPoint &p) { userIDTableContextMenuRequested(p); });
    connect(ui.addUserIDBtn, &QPushButton::clicked,
            q, [this]() { addUserID(); });
    connect(ui.changePassphraseBtn, &QPushButton::clicked,
            q, [this]() { changePassphrase(); });
    connect(ui.genRevokeBtn, &QPushButton::clicked,
            q, [this]() { genRevokeCert(); });
    connect(ui.changeExpirationAction, &QAction::triggered,
            q, [this]() { changeExpiration(); });
    connect(ui.showIssuerCertificateAction, &QAction::triggered,
            q, [this]() { showIssuerCertificate(); });
    connect(ui.trustChainDetailsBtn, &QPushButton::pressed,
            q, [this]() { showTrustChainDialog(); });
    connect(ui.moreDetailsBtn, &QPushButton::pressed,
            q, [this]() { showMoreDetails(); });
    connect(ui.refreshBtn, &QPushButton::clicked,
            q, [this]() { refreshCertificate(); });
    connect(ui.certifyBtn, &QPushButton::clicked,
            q, [this]() { certifyClicked(); });
    connect(ui.webOfTrustBtn, &QPushButton::clicked,
            q, [this]() { webOfTrustClicked(); });
    connect(ui.exportBtn, &QPushButton::clicked,
            q, [this]() { exportClicked(); });
    if (ui.copyFingerprintAction) {
        connect(ui.copyFingerprintAction, &QAction::triggered,
                q, [this]() { copyFingerprintToClipboard(); });
    }

    connect(Kleo::KeyCache::instance().get(), &Kleo::KeyCache::keysMayHaveChanged,
            q, [this]() { keysMayHaveChanged(); });
}

void CertificateDetailsWidget::Private::setupCommonProperties()
{
    const bool hasSecret = key.hasSecret();
    const bool isOpenPGP = key.protocol() == GpgME::OpenPGP;

    ui.changePassphraseBtn->setVisible(hasSecret);
    ui.genRevokeBtn->setVisible(isOpenPGP && hasSecret);
    ui.certifyBtn->setVisible(isOpenPGP && !hasSecret);
    if (isOpenPGP && hasSecret) {
        ui.expiresField->setAction(ui.changeExpirationAction);
    } else {
        ui.expiresField->setAction(nullptr);
    }
    ui.addUserIDBtn->setVisible(hasSecret && isOpenPGP);
    ui.webOfTrustBtn->setVisible(isOpenPGP);

    ui.validFromField->setValue(Kleo::Formatting::creationDateString(key));
    const QString expiry = Kleo::Formatting::expirationDateString(key);
    ui.expiresField->setValue(expiry.isEmpty() ? i18nc("Expires", "never") : expiry);
    ui.typeField->setValue(Kleo::Formatting::type(key));
    ui.fingerprintField->setValue(Formatting::prettyID(key.primaryFingerprint()));

    ui.complianceField->setVisible(Kleo::gnupgIsDeVsCompliant());
    if (Kleo::gnupgIsDeVsCompliant()) {
        ui.complianceField->setValue(Kleo::Formatting::complianceStringForKey(key));
    }

    ui.userIDTable->clear();

    QStringList headers = { i18n("Email"), i18n("Name"), i18n("Trust Level"), i18n("Tags") };
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
        for (const auto &tag: uid.remarks(Tags::tagKeys(), err)) {
            if (err) {
                qCWarning(KLEOPATRA_LOG) << "Getting remarks for user ID" << uid.id() << "failed:" << err;
            }
            tagList << QString::fromStdString(tag);
        }
        qCDebug(KLEOPATRA_LOG) << "tagList:" << tagList;
        const auto tags = tagList.join(QStringLiteral("; "));
        item->setData(3, Qt::DisplayRole, tags);
        item->setData(3, Qt::ToolTipRole, toolTip);

        ui.userIDTable->addTopLevelItem(item);
    }
    if (!Tags::tagsEnabled()) {
        ui.userIDTable->hideColumn(3);
    }
}

void CertificateDetailsWidget::Private::revokeUserID(const GpgME::UserID &userId)
{
    const QString message = xi18nc(
        "@info",
        "<para>Do you really want to revoke the user ID<nl/><emphasis>%1</emphasis> ?</para>",
        QString::fromUtf8(userId.id()));
    auto confirmButton = KStandardGuiItem::yes();
    confirmButton.setText(i18nc("@action:button", "Revoke User ID"));
    confirmButton.setToolTip({});
    const auto choice = KMessageBox::questionYesNo(
        q->window(), message, i18nc("@title:window", "Confirm Revocation"),
        confirmButton, KStandardGuiItem::cancel(), {}, KMessageBox::Notify | KMessageBox::WindowModal);
    if (choice != KMessageBox::Yes) {
        return;
    }

    auto cmd = new Commands::RevokeUserIDCommand(userId);
    cmd->setParentWidget(q);
    ui.userIDTable->setEnabled(false);
    connect(cmd, &Command::finished,
            q, [this]() {
        ui.userIDTable->setEnabled(true);
        updateKey();
    });
    cmd->start();
}

void CertificateDetailsWidget::Private::changeExpiration()
{
    auto cmd = new Kleo::Commands::ChangeExpiryCommand(key);
    QObject::connect(cmd, &Kleo::Commands::ChangeExpiryCommand::finished,
                     q, [this]() {
                         ui.changeExpirationAction->setEnabled(true);
                     });
    ui.changeExpirationAction->setEnabled(false);
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

void CertificateDetailsWidget::Private::refreshCertificate()
{
    auto cmd = new Kleo::RefreshCertificateCommand{key};
    QObject::connect(cmd, &Kleo::RefreshCertificateCommand::finished,
                     q, [this]() {
                         ui.refreshBtn->setEnabled(true);
                     });
    ui.refreshBtn->setEnabled(false);
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
                         updateKey();
                     });
    ui.addUserIDBtn->setEnabled(false);
    cmd->start();
}

namespace
{
void ensureThatKeyDetailsAreLoaded(GpgME::Key &key)
{
    if (key.userID(0).numSignatures() == 0) {
        key.update();
    }
}
}

void CertificateDetailsWidget::Private::keysMayHaveChanged()
{
    auto newKey = Kleo::KeyCache::instance()->findByFingerprint(key.primaryFingerprint());
    if (!newKey.isNull()) {
        ensureThatKeyDetailsAreLoaded(newKey);
        setUpdatedKey(newKey);
    }
}

void CertificateDetailsWidget::Private::showTrustChainDialog()
{
    QScopedPointer<TrustChainDialog> dlg(new TrustChainDialog(q));
    dlg->setKey(key);
    dlg->exec();
}

namespace
{
bool isLastValidUserID(const GpgME::UserID &userId)
{
    if (isRevokedOrExpired(userId)) {
        return false;
    }
    const auto userIds = userId.parent().userIDs();
    const int numberOfValidUserIds = std::count_if(std::begin(userIds), std::end(userIds),
                                                   [](const auto &u) {
                                                       return !isRevokedOrExpired(u);
                                                   });
    return numberOfValidUserIds == 1;
}

bool canRevokeUserID(const GpgME::UserID &userId)
{
    const auto key = userId.parent();
    return (!userId.isNull() //
            && key.protocol() == GpgME::OpenPGP
            && canCreateCertifications(key)
            && !isLastValidUserID(userId));
}
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
            updateKey();
        });
        cmd->start();
    });
    {
        auto action = menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-revoke")),
                                      i18nc("@action:inmenu", "Revoke User ID"),
                                      q, [this, userID]() {
                                          revokeUserID(userID);
                                      });
        action->setEnabled(canRevokeUserID(userID));
    }
    if (Kleo::Commands::RevokeCertificationCommand::isSupported()) {
        menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-revoke")),
                        i18n("Revoke Certification..."),
                        q, [this, userID]() {
            auto cmd = new Kleo::Commands::RevokeCertificationCommand(userID);
            ui.userIDTable->setEnabled(false);
            connect(cmd, &Kleo::Commands::RevokeCertificationCommand::finished,
                    q, [this]() {
                ui.userIDTable->setEnabled(true);
                updateKey();
            });
            cmd->start();
        });
    }
#ifdef MAILAKONADI_ENABLED
    if (key.hasSecret() && key.protocol() == GpgME::OpenPGP) {
        menu->addAction(QIcon::fromTheme(QStringLiteral("view-certificate-export")),
                        i18nc("@action:inmenu", "Publish at Mail Provider ..."),
                        q, [this, userID]() {
            auto cmd = new Kleo::Commands::ExportOpenPGPCertToProviderCommand(userID);
            ui.userIDTable->setEnabled(false);
            connect(cmd, &Kleo::Commands::ExportOpenPGPCertToProviderCommand::finished,
                    q, [this]() { ui.userIDTable->setEnabled(true); });
            cmd->start();
        });
    }
#endif // MAILAKONADI_ENABLED
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

void CertificateDetailsWidget::Private::setupPGPProperties()
{
    ui.smimeOwnerField->setVisible(false);
    ui.smimeIssuerField->setVisible(false);
    ui.smimeRelatedAddresses->setVisible(false);
    ui.trustChainDetailsBtn->setVisible(false);

    ui.userIDTable->setContextMenuPolicy(Qt::CustomContextMenu);

    const auto trustDomains = accumulateTrustDomains(key.userIDs());
    ui.trustedIntroducerField->setVisible(!trustDomains.empty());
    ui.trustedIntroducerField->setValue(QStringList(std::begin(trustDomains), std::end(trustDomains)).join(u", "));

    ui.refreshBtn->setToolTip(i18nc("@info:tooltip", "Update the key from external sources."));
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
    ui.trustedIntroducerField->setVisible(false);

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
    ui.smimeOwnerField->setValue(owner);

    const Kleo::DN issuerDN(key.issuerName());
    const QString issuerCN = issuerDN[QStringLiteral("CN")];
    const QString issuer = issuerCN.isEmpty() ? QString::fromUtf8(key.issuerName()) : issuerCN;
    ui.smimeIssuerField->setValue(issuer);

    ui.smimeIssuerField->setToolTip(formatDNToolTip(issuerDN));

    ui.smimeOwnerField->setToolTip(formatDNToolTip(dn));

    ui.refreshBtn->setToolTip(i18nc("@info:tooltip", "Update the CRLs and do a full validation check of the certificate."));
}

void CertificateDetailsWidget::Private::showIssuerCertificate()
{
    const auto parentKey = KeyCache::instance()->findIssuers(key, KeyCache::NoOption);

    if (!parentKey.size()) {
        return;
    }
    auto cmd = new Kleo::Commands::DetailsCommand(parentKey[0], nullptr);
    cmd->setParentWidget(q);
    cmd->start();
}

void CertificateDetailsWidget::Private::copyFingerprintToClipboard()
{
    if (auto clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(QString::fromLatin1(key.primaryFingerprint()));
    }
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

void CertificateDetailsWidget::Private::updateKey()
{
    key.update();
    setUpdatedKey(key);
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
    if (key.hasSecret()) {
        ctx->addKeyListMode(GpgME::WithSecret);
    }

    // Windows QGpgME new style connect problem makes this necessary.
    connect(job, SIGNAL(result(GpgME::KeyListResult,std::vector<GpgME::Key>,QString,GpgME::Error)),
            this, SLOT(keyListDone(GpgME::KeyListResult,std::vector<GpgME::Key>,QString,GpgME::Error)));

    job->start(QStringList() << QLatin1String(key.primaryFingerprint()));
}

GpgME::Key CertificateDetailsWidget::key() const
{
    return d->key;
}

#include "moc_certificatedetailswidget.cpp"
