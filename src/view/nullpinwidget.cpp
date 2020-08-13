/*  view/nullpinwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "nullpinwidget.h"

#include "kleopatra_debug.h"

#include "smartcard/readerstatus.h"

#include <gpgme++/error.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

#include <KLocalizedString>
#include <KMessageBox>

using namespace Kleo;
using namespace Kleo::SmartCard;

NullPinWidget::NullPinWidget(QWidget *parent)
    : QWidget(parent)
{
    const auto nullTitle = i18nc("NullPIN is a word that is used all over in the netkey "
                                 "documentation and should be understandable by Netkey cardholders",
                                 "The NullPIN is still active on this card.");
    const auto nullDescription = i18n("You need to set a PIN before you can use the certificates.");
    const auto descriptionLbl = new QLabel(QStringLiteral("<b>%1</b><br/>%2").arg(nullTitle, nullDescription));

    auto vLay = new QVBoxLayout(this);
    vLay->addWidget(descriptionLbl, 0, Qt::AlignCenter);

    mNKSBtn = new QPushButton(i18nc("NKS is an identifier for a type of keys on a NetKey card", "Set NKS PIN"));
    mSigGBtn = new QPushButton(i18nc("SigG is an identifier for a type of keys on a NetKey card", "Set SigG PIN"));

    connect(mNKSBtn, &QPushButton::clicked, this, [this] () {
            mNKSBtn->setEnabled(false);
            doChangePin(false);
        });
    connect(mSigGBtn, &QPushButton::clicked, this, [this] () {
            mSigGBtn->setEnabled(false);
            doChangePin(true);
        });

    auto hLayBtn = new QHBoxLayout;
    hLayBtn->addStretch(1);
    hLayBtn->addWidget(mNKSBtn);
    hLayBtn->addWidget(mSigGBtn);
    hLayBtn->addStretch(1);

    vLay->addLayout(hLayBtn);
}

void NullPinWidget::doChangePin(bool sigG)
{
    auto ret = KMessageBox::warningContinueCancel(this,
            i18n("Setting a PIN is required but <b>can't be reverted</b>.") +
            QStringLiteral("<p>%1</p><p>%2</p>").arg(
                i18n("If you proceed you will be asked to enter a new PIN "
                     "and later to repeat that PIN.")).arg(
                i18n("It will <b>not be possible</b> to recover the "
                     "card if the PIN has been entered wrongly more than 2 times.")),
            i18n("Set initial PIN"),
            KStandardGuiItem::cont(),
            KStandardGuiItem::cancel());

    if (ret != KMessageBox::Continue) {
        return;
    }

    if (sigG) {
        ReaderStatus::mutableInstance()
        ->startSimpleTransaction("SCD PASSWD --nullpin PW1.CH.SIG",
                                 this, "setSigGPinSettingResult");
    } else {
        ReaderStatus::mutableInstance()
        ->startSimpleTransaction("SCD PASSWD --nullpin PW1.CH",
                                 this, "setNksPinSettingResult");
    }
}

void NullPinWidget::handleResult(const GpgME::Error &err, QPushButton *btn)
{
    btn->setEnabled(true);
    if (err.isCanceled()) {
        return;
    }
    if (err) {
        KMessageBox::error(this, i18nc("@info",
                           "Failed to set PIN: %1", QString::fromLatin1(err.asString())),
                           i18nc("@title", "Error"));
        return;
    }
    btn->setVisible(false);

    if (!mNKSBtn->isVisible() && !mSigGBtn->isVisible()) {
        // Both pins are set, we can hide.
        setVisible(false);
    }
}

void NullPinWidget::setSigGVisible(bool val)
{
    mSigGBtn->setVisible(val);
}

void NullPinWidget::setNKSVisible(bool val)
{
    mNKSBtn->setVisible(val);
}

void NullPinWidget::setSigGPinSettingResult(const GpgME::Error &err)
{
    handleResult(err, mSigGBtn);
}

void NullPinWidget::setNksPinSettingResult(const GpgME::Error &err)
{
    handleResult(err, mNKSBtn);
}
