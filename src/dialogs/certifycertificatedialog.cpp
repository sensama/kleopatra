/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/signcertificatedialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2019 g10code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "kleopatra_debug.h"

#include "certifycertificatedialog.h"

#include "certifywidget.h"

#include <Libkleo/Formatting>
#include <Libkleo/Stl_Util>

#include <KLocalizedString>
#include <KStandardGuiItem>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KMessageBox>

#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QPushButton>
#include <QToolTip>

#include <gpg-error.h>


using namespace GpgME;
using namespace Kleo;

CertifyCertificateDialog::CertifyCertificateDialog(QWidget *p, Qt::WindowFlags f)
    : QDialog(p, f)
{
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));

    // Setup GUI
    auto mainLay = new QVBoxLayout(this);
    mCertWidget = new CertifyWidget(this);
    mainLay->addWidget(mCertWidget);

    auto buttonBox = new QDialogButtonBox{this};
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel |
                                  QDialogButtonBox::Ok);
    const auto okButton = buttonBox->button(QDialogButtonBox::Ok);
    KGuiItem::assign(okButton, KStandardGuiItem::ok());
    okButton->setText(i18n("Certify"));
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());
    connect(buttonBox, &QDialogButtonBox::accepted, this, &CertifyCertificateDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::close);

    mainLay->addWidget(buttonBox);

    okButton->setEnabled(mCertWidget->isValid());
    connect(mCertWidget, &CertifyWidget::changed, this, [this, okButton] () {
        okButton->setEnabled(mCertWidget->isValid());
    });

    KConfigGroup cfgGroup(KSharedConfig::openStateConfig(), "CertifyDialog");
    const QByteArray geom = cfgGroup.readEntry("geometry", QByteArray());
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
        return;
    }
    resize(QSize(640, 480));
}

CertifyCertificateDialog::~CertifyCertificateDialog()
{
    KConfigGroup cfgGroup(KSharedConfig::openStateConfig(), "CertifyDialog");
    cfgGroup.writeEntry("geometry", saveGeometry());
    cfgGroup.sync();
}

void CertifyCertificateDialog::setCertificateToCertify(const Key &key)
{
    setWindowTitle(i18nc("@title:window arg is name, email of certificate holder", "Certify Certificate: %1", Formatting::prettyName(key)));
    mCertWidget->setTarget(key);
}

bool CertifyCertificateDialog::exportableCertificationSelected() const
{
    return mCertWidget->exportableSelected();
}

bool CertifyCertificateDialog::trustSignatureSelected() const
{
    return mCertWidget->trustSignatureSelected();
}

QString CertifyCertificateDialog::trustSignatureDomain() const
{
    return mCertWidget->trustSignatureDomain();
}

bool CertifyCertificateDialog::nonRevocableCertificationSelected() const
{
    return false;
}

Key CertifyCertificateDialog::selectedSecretKey() const
{
    return mCertWidget->secKey();
}

bool CertifyCertificateDialog::sendToServer() const
{
    return mCertWidget->publishSelected();
}

unsigned int CertifyCertificateDialog::selectedCheckLevel() const
{
    //PENDING
#ifdef KLEO_SIGN_KEY_CERTLEVEL_SUPPORT
    return d->selectCheckLevelPage->checkLevel();
#endif
    return 0;
}

void CertifyCertificateDialog::setSelectedUserIDs(const std::vector<UserID> &uids)
{
    mCertWidget->selectUserIDs(uids);
}

std::vector<unsigned int> CertifyCertificateDialog::selectedUserIDs() const
{
    return mCertWidget->selectedUserIDs();
}

QString CertifyCertificateDialog::tags() const
{
    return mCertWidget->tags();
}

QDate CertifyCertificateDialog::expirationDate() const
{
    return mCertWidget->expirationDate();
}

void CertifyCertificateDialog::accept()
{
    if (!mCertWidget->isValid()) {
        return;
    }

    KConfigGroup conf(KSharedConfig::openConfig(), "CertifySettings");
    const auto lastKey = mCertWidget->secKey();
    if (!lastKey.isNull()) {
        conf.writeEntry("LastKey", lastKey.primaryFingerprint());
    }
    conf.writeEntry("ExportCheckState", mCertWidget->exportableSelected());
    conf.writeEntry("PublishCheckState", mCertWidget->publishSelected());

    QDialog::accept();
}

void CertifyCertificateDialog::keyPressEvent(QKeyEvent *e)
{
    // if Escape is pressed while a tool tip is shown, then close the tool tip but not the dialog
    if (e->matches(QKeySequence::Cancel) && QToolTip::isVisible()) {
        QToolTip::hideText();
        return;
    }

    QDialog::keyPressEvent(e);
}
