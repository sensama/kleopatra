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
    ~Private();

private:
    void saveGeometry();
    void restoreGeometry(const QSize &defaultSize);

private:
    RevokeCertificationWidget *mainWidget = nullptr;
};


RevokeCertificationDialog::Private::Private(RevokeCertificationDialog *qq)
    : q(qq)
{
}

RevokeCertificationDialog::Private::~Private()
{
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

    auto buttonBox = new QDialogButtonBox();
    mainLay->addWidget(buttonBox);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel |
                                  QDialogButtonBox::Ok);
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Ok), KStandardGuiItem::ok());
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());
    buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("Revoke Certification"));
    connect(buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::clicked,
            this, [this] () {
                d->mainWidget->saveConfig();
                accept();
            });
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked,
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
