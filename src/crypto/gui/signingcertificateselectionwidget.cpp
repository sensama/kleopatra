/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/signingcertificateselectionwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007, 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "signingcertificateselectionwidget.h"

#include "ui_signingcertificateselectionwidget.h"

#include <Libkleo/KeyCache>
#include <Libkleo/Formatting>
#include <Libkleo/Stl_Util>

#include <QByteArray>
#include <QMap>


using namespace Kleo;
using namespace Kleo::Crypto::Gui;

class SigningCertificateSelectionWidget::Private
{
    friend class ::SigningCertificateSelectionWidget;
    SigningCertificateSelectionWidget *const q;
public:
    explicit Private(SigningCertificateSelectionWidget *qq);
    ~Private();
    static std::vector<GpgME::Key> candidates(GpgME::Protocol prot);
    static void addCandidates(GpgME::Protocol prot, QComboBox *combo);

private:
    Ui::SigningCertificateSelectionWidget ui;
};

static GpgME::Key current_cert(const QComboBox &cb)
{
    const QByteArray fpr = cb.itemData(cb.currentIndex()).toByteArray();
    return KeyCache::instance()->findByFingerprint(fpr.constData());
}

static void select_cert(QComboBox &cb, const GpgME::Key &key)
{
    const QByteArray fpr = key.primaryFingerprint();
    if (!fpr.isEmpty()) {
        cb.setCurrentIndex(cb.findData(fpr));
    }
}

static void add_cert(QComboBox &cb, const GpgME::Key &key)
{
    cb.addItem(Formatting::formatForComboBox(key),
               QVariant(QByteArray(key.primaryFingerprint())));

}

SigningCertificateSelectionWidget::Private::Private(SigningCertificateSelectionWidget *qq)
    : q(qq), ui()
{
    ui.setupUi(q);
    addCandidates(GpgME::CMS, ui.cmsCombo);
    addCandidates(GpgME::OpenPGP, ui.pgpCombo);
    ui.rememberCO->setChecked(true);
}

SigningCertificateSelectionWidget::Private::~Private() {}

SigningCertificateSelectionWidget::SigningCertificateSelectionWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f), d(new Private(this))
{

}

SigningCertificateSelectionWidget::~SigningCertificateSelectionWidget() {}

void SigningCertificateSelectionWidget::setSelectedCertificates(const QMap<GpgME::Protocol, GpgME::Key> &certificates)
{
    setSelectedCertificates(certificates[GpgME::OpenPGP], certificates[GpgME::CMS]);
}

void SigningCertificateSelectionWidget::setSelectedCertificates(const GpgME::Key &pgp, const GpgME::Key &cms)
{
    select_cert(*d->ui.pgpCombo, pgp);
    select_cert(*d->ui.cmsCombo, cms);
}

std::vector<GpgME::Key> SigningCertificateSelectionWidget::Private::candidates(GpgME::Protocol prot)
{
    Q_ASSERT(prot != GpgME::UnknownProtocol);
    std::vector<GpgME::Key> keys = KeyCache::instance()->keys();
    std::vector<GpgME::Key>::iterator end = keys.end();

    end = std::remove_if(keys.begin(), end, [prot](const GpgME::Key &key) { return key.protocol() != prot; });
    end = std::remove_if(keys.begin(), end, [](const GpgME::Key &key) { return !key.hasSecret(); });
    Q_ASSERT(std::all_of(keys.begin(), end, [](const GpgME::Key &key) { return key.hasSecret(); }));
    end = std::remove_if(keys.begin(), end, [](const GpgME::Key &key) { return !key.canReallySign(); });
    end = std::remove_if(keys.begin(), end, [](const GpgME::Key &key) { return key.isExpired(); });
    end = std::remove_if(keys.begin(), end, [](const GpgME::Key &key) { return key.isRevoked(); });
    keys.erase(end, keys.end());
    return keys;
}

void SigningCertificateSelectionWidget::Private::addCandidates(GpgME::Protocol prot, QComboBox *combo)
{
    const std::vector<GpgME::Key> keys = candidates(prot);
    for (const GpgME::Key &i : keys) {
        add_cert(*combo, i);
    }
}

QMap<GpgME::Protocol, GpgME::Key> SigningCertificateSelectionWidget::selectedCertificates() const
{
    QMap<GpgME::Protocol, GpgME::Key> res;

    res.insert(GpgME::OpenPGP, current_cert(*d->ui.pgpCombo));
    res.insert(GpgME::CMS,     current_cert(*d->ui.cmsCombo));

    return res;
}

bool SigningCertificateSelectionWidget::rememberAsDefault() const
{
    return d->ui.rememberCO->isChecked();
}

void SigningCertificateSelectionWidget::setAllowedProtocols(const QVector<GpgME::Protocol> &allowedProtocols)
{
    setAllowedProtocols(allowedProtocols.contains(GpgME::OpenPGP),
                        allowedProtocols.contains(GpgME::CMS));
}

void SigningCertificateSelectionWidget::setAllowedProtocols(bool pgp, bool cms)
{
    d->ui.pgpLabel->setVisible(pgp);
    d->ui.pgpCombo->setVisible(pgp);

    d->ui.cmsLabel->setVisible(cms);
    d->ui.cmsCombo->setVisible(cms);
}

