/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/signcertificatedialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB
                  2019 g10code GmbH

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

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
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
#include <QPushButton>

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

    QDialogButtonBox *buttonBox = new QDialogButtonBox();
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel |
                                  QDialogButtonBox::Ok);
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Ok), KStandardGuiItem::ok());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());

    buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("Certify"));
    connect(buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::clicked,
            this, [this] () {
        KConfigGroup conf(KSharedConfig::openConfig(), "CertifySettings");
        const auto lastKey = mCertWidget->secKey();
        // Do not accept if the keys are the same.
        if (!lastKey.isNull() && !mCertWidget->target().isNull() &&
            !strcmp(lastKey.primaryFingerprint(),
                    mCertWidget->target().primaryFingerprint())) {
            KMessageBox::error(this, i18n("You cannot certify using the same key."),
                               i18n("Invalid Selection"), KMessageBox::Notify);
            return;
        }

        if (!lastKey.isNull()) {
            conf.writeEntry("LastKey", lastKey.primaryFingerprint());
        }
        conf.writeEntry("ExportCheckState", mCertWidget->exportableSelected());
        conf.writeEntry("PublishCheckState", mCertWidget->publishSelected());
        accept();
    });
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked,
            this, [this] () {
        close();
    });

    mainLay->addWidget(buttonBox);
    KConfigGroup cfgGroup(KSharedConfig::openConfig(), "CertifyDialog");
    const QByteArray geom = cfgGroup.readEntry("geometry", QByteArray());
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
        return;
    }
    resize(QSize(640, 480));
}

CertifyCertificateDialog::~CertifyCertificateDialog()
{
    KConfigGroup cfgGroup(KSharedConfig::openConfig(), "CertifyDialog");
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

bool CertifyCertificateDialog::trustCertificationSelected() const
{
    return false;
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

QString CertifyCertificateDialog::remarks() const
{
    return mCertWidget->remarks();
}
