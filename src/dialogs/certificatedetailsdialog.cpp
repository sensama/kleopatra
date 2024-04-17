/*  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "certificatedetailsdialog.h"

#include "certificatedetailswidget.h"
#include "commands/changepassphrasecommand.h"
#include "commands/dumpcertificatecommand.h"
#include "commands/genrevokecommand.h"
#include "commands/refreshcertificatescommand.h"
#include "exportdialog.h"

#include <Libkleo/KeyHelpers>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QStringBuilder>
#include <QVBoxLayout>

#include <gpgme++/key.h>

using namespace Kleo;

class CertificateDetailsDialog::Private
{
public:
    Private(CertificateDetailsDialog *qq)
        : q(qq)
    {
    }
    QPushButton *changePassphraseBtn = nullptr;
    QPushButton *genRevokeBtn = nullptr;
    QPushButton *exportBtn = nullptr;
    QPushButton *refreshBtn = nullptr;
    void refreshCertificate();
    void exportClicked();
    void genRevokeCert();
    void changePassphrase();
    void showMoreDetails();

private:
    CertificateDetailsDialog *q;
};

CertificateDetailsDialog::CertificateDetailsDialog(QWidget *parent)
    : QDialog(parent)
    , d(new Private(this))
{
    auto l = new QVBoxLayout(this);
    auto w = new CertificateDetailsWidget{this};
    w->layout()->setContentsMargins(0, 0, 0, 0);
    l->addWidget(w);

    auto bbox = new QDialogButtonBox(this);

    d->refreshBtn = new QPushButton{i18nc("@action:button", "Update"), parent};
    bbox->addButton(d->refreshBtn, QDialogButtonBox::ActionRole);

    d->exportBtn = new QPushButton(i18nc("@action:button", "Export"), parent);
    bbox->addButton(d->exportBtn, QDialogButtonBox::ActionRole);

    d->genRevokeBtn = new QPushButton(i18nc("@action:button", "Generate Revocation Certificate"), parent);
    d->genRevokeBtn->setToolTip(u"<html>"
                                % i18n("A revocation certificate is a file that serves as a \"kill switch\" to publicly "
                                       "declare that a key shall not anymore be used.  It is not possible "
                                       "to retract such a revocation certificate once it has been published.")
                                % u"</html>");
    bbox->addButton(d->genRevokeBtn, QDialogButtonBox::ActionRole);

    d->changePassphraseBtn = new QPushButton(i18nc("@action:button", "Change Passphrase"), parent);
    bbox->addButton(d->changePassphraseBtn, QDialogButtonBox::ActionRole);

    auto closeBtn = bbox->addButton(QDialogButtonBox::Close);
    connect(closeBtn, &QPushButton::pressed, this, &QDialog::accept);
    l->addWidget(bbox);

    connect(d->exportBtn, &QPushButton::clicked, this, [this]() {
        d->exportClicked();
    });

    connect(d->refreshBtn, &QPushButton::clicked, this, [this]() {
        d->refreshCertificate();
    });
    connect(d->genRevokeBtn, &QPushButton::clicked, this, [this]() {
        d->genRevokeCert();
    });
    connect(d->changePassphraseBtn, &QPushButton::clicked, this, [this]() {
        d->changePassphrase();
    });
    readConfig();
}

CertificateDetailsDialog::~CertificateDetailsDialog()
{
    writeConfig();
}

void CertificateDetailsDialog::readConfig()
{
    KConfigGroup dialog(KSharedConfig::openStateConfig(), "CertificateDetailsDialog");
    const QSize size = dialog.readEntry("Size", QSize(730, 280));
    if (size.isValid()) {
        resize(size);
    }
}

void CertificateDetailsDialog::writeConfig()
{
    KConfigGroup dialog(KSharedConfig::openStateConfig(), "CertificateDetailsDialog");
    dialog.writeEntry("Size", size());
    dialog.sync();
}

namespace
{
QString title(const GpgME::Key &key)
{
    switch (key.protocol()) {
    case GpgME::OpenPGP:
        return i18nc("@title:window", "OpenPGP Certificate");
    case GpgME::CMS:
        return i18nc("@title:window", "S/MIME Certificate");
    default:
        return {};
    }
}
}

void CertificateDetailsDialog::setKey(const GpgME::Key &key)
{
    setWindowTitle(title(key));
    findChild<CertificateDetailsWidget *>()->setKey(key);
    d->exportBtn->setVisible(!isRemoteKey(key));
    d->refreshBtn->setVisible(!isRemoteKey(key));

    if (key.protocol() == GpgME::Protocol::CMS) {
        d->refreshBtn->setToolTip(i18nc("@info:tooltip", "Update the CRLs and do a full validation check of the certificate."));
    } else {
        d->refreshBtn->setToolTip(i18nc("@info:tooltip", "Update the key from external sources."));
    }
    d->genRevokeBtn->setVisible(key.protocol() == GpgME::Protocol::OpenPGP && key.hasSecret());
    d->genRevokeBtn->setEnabled(canBeUsedForSecretKeyOperations(key));
    d->changePassphraseBtn->setVisible(isSecretKeyStoredInKeyRing(key));
}

GpgME::Key CertificateDetailsDialog::key() const
{
    return findChild<CertificateDetailsWidget *>()->key();
}

void CertificateDetailsDialog::Private::exportClicked()
{
    QScopedPointer<ExportDialog> dlg(new ExportDialog(q));
    dlg->setKey(q->key());
    dlg->exec();
}

void CertificateDetailsDialog::Private::refreshCertificate()
{
    auto cmd = new Kleo::RefreshCertificatesCommand(q->key());
    QObject::connect(cmd, &Kleo::RefreshCertificatesCommand::finished, q, [this]() {
        refreshBtn->setEnabled(true);
    });
    refreshBtn->setEnabled(false);
    cmd->start();
}

void CertificateDetailsDialog::Private::genRevokeCert()
{
    auto cmd = new Kleo::Commands::GenRevokeCommand(q->key());
    QObject::connect(cmd, &Kleo::Commands::GenRevokeCommand::finished, q, [this]() {
        genRevokeBtn->setEnabled(true);
    });
    genRevokeBtn->setEnabled(false);
    cmd->start();
}

void CertificateDetailsDialog::Private::changePassphrase()
{
    auto cmd = new Kleo::Commands::ChangePassphraseCommand(q->key());
    QObject::connect(cmd, &Kleo::Commands::ChangePassphraseCommand::finished, q, [this]() {
        changePassphraseBtn->setEnabled(true);
    });
    changePassphraseBtn->setEnabled(false);
    cmd->start();
}
