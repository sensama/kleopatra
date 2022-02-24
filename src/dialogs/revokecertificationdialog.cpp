/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/revokecertificationdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "revokecertificationdialog.h"

#include "revokecertificationwidget.h"

#include <Libkleo/Formatting>

#include <KConfigGroup>
#include <KGuiItem>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KStandardGuiItem>

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "kleopatra_debug.h"

using namespace GpgME;
using namespace Kleo;

class RevokeCertificationDialog::Private
{
    friend class ::Kleo::RevokeCertificationDialog;
    RevokeCertificationDialog *const q;
public:
    explicit Private(RevokeCertificationDialog *qq);

    void updateOkButton();

private:
    void saveGeometry();
    void restoreGeometry(const QSize &defaultSize);

private:
    RevokeCertificationWidget *mainWidget = nullptr;
    QPushButton *okButton = nullptr;
};


RevokeCertificationDialog::Private::Private(RevokeCertificationDialog *qq)
    : q(qq)
{
}

void RevokeCertificationDialog::Private::updateOkButton()
{
    okButton->setEnabled(!mainWidget->certificationKey().isNull()
                         && !mainWidget->selectedUserIDs().empty());
}

void RevokeCertificationDialog::Private::saveGeometry()
{
    KConfigGroup cfgGroup(KSharedConfig::openStateConfig(), "RevokeCertificationDialog");
    cfgGroup.writeEntry("geometry", q->saveGeometry());
    cfgGroup.sync();
}

void RevokeCertificationDialog::Private::restoreGeometry(const QSize &defaultSize)
{
    KConfigGroup cfgGroup(KSharedConfig::openStateConfig(), "RevokeCertificationDialog");
    const QByteArray geometry = cfgGroup.readEntry("geometry", QByteArray());
    if (!geometry.isEmpty()) {
        q->restoreGeometry(geometry);
    } else {
        q->resize(defaultSize);
    }
}

RevokeCertificationDialog::RevokeCertificationDialog(QWidget *p, Qt::WindowFlags f)
    : QDialog(p, f)
    , d(new Private(this))
{
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));

    auto mainLay = new QVBoxLayout(this);
    d->mainWidget = new RevokeCertificationWidget(this);
    mainLay->addWidget(d->mainWidget);

    auto buttonBox = new QDialogButtonBox(this);
    mainLay->addWidget(buttonBox);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel |
                                  QDialogButtonBox::Ok);
    d->okButton = buttonBox->button(QDialogButtonBox::Ok);
    auto cancelButton = buttonBox->button(QDialogButtonBox::Cancel);
    KGuiItem::assign(d->okButton, KStandardGuiItem::ok());
    KGuiItem::assign(cancelButton, KStandardGuiItem::cancel());
    d->okButton->setText(i18n("Revoke Certification"));

    connect(d->mainWidget, &RevokeCertificationWidget::certificationKeyChanged,
            this, [this]() { d->updateOkButton(); });
    connect(d->mainWidget, &RevokeCertificationWidget::selectedUserIDsChanged,
            this, [this]() { d->updateOkButton(); });
    d->updateOkButton();

    connect(d->okButton, &QAbstractButton::clicked,
            this, [this] () {
                d->mainWidget->saveConfig();
                accept();
            });
    connect(cancelButton, &QAbstractButton::clicked,
            this, [this] () { close(); });

    d->restoreGeometry(QSize(640, 480));
}

RevokeCertificationDialog::~RevokeCertificationDialog()
{
    d->saveGeometry();
}

void RevokeCertificationDialog::setCertificateToRevoke(const Key &key)
{
    setWindowTitle(i18nc("@title:window arg is name, email of certificate holder",
                         "Revoke Certification: %1", Formatting::prettyName(key)));
    d->mainWidget->setTarget(key);
}

void RevokeCertificationDialog::setSelectedUserIDs(const std::vector<UserID> &uids)
{
    d->mainWidget->setSelectUserIDs(uids);
}

std::vector<GpgME::UserID> RevokeCertificationDialog::selectedUserIDs() const
{
    return d->mainWidget->selectedUserIDs();
}

void Kleo::RevokeCertificationDialog::setSelectedCertificationKey(const GpgME::Key &key)
{
    d->mainWidget->setCertificationKey(key);
}

Key RevokeCertificationDialog::selectedCertificationKey() const
{
    return d->mainWidget->certificationKey();
}

bool RevokeCertificationDialog::sendToServer() const
{
    return d->mainWidget->publishSelected();
}
