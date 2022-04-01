/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/revokekeydialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "revokekeydialog.h"

#include <Libkleo/Formatting>

#include <KConfigGroup>
#include <KGuiItem>
#include <KLocalizedString>
#include <KSeparator>
#include <KSharedConfig>
#include <KStandardGuiItem>

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QTextEdit>
#include <QVBoxLayout>

#ifdef QGPGME_SUPPORTS_KEY_REVOCATION
#include <gpgme++/global.h>
#endif
#include <gpgme++/key.h>

#include <kleopatra_debug.h>

using namespace Kleo;
using namespace GpgME;

class RevokeKeyDialog::Private
{
    friend class ::Kleo::RevokeKeyDialog;
    RevokeKeyDialog *const q;

    struct {
        QLabel *infoLabel = nullptr;
        QTextEdit *description = nullptr;
        QDialogButtonBox *buttonBox = nullptr;
    } ui;

    Key key;
    QButtonGroup reasonGroup;

public:
    Private(RevokeKeyDialog *qq)
        : q(qq)
    {
        q->setWindowTitle(i18nc("title:window", "Revoke Key"));

        auto mainLayout = new QVBoxLayout{q};

        ui.infoLabel = new QLabel{q};
        mainLayout->addWidget(ui.infoLabel);

#ifdef QGPGME_SUPPORTS_KEY_REVOCATION
        auto groupBox = new QGroupBox{i18nc("@title:group", "Reason for revocation"), q};

        reasonGroup.addButton(new QRadioButton{i18nc("@option:radio", "No reason specified"), q},
                              static_cast<int>(RevocationReason::Unspecified));
        reasonGroup.addButton(new QRadioButton{i18nc("@option:radio", "Key has been compromised"), q},
                              static_cast<int>(RevocationReason::Compromised));
        reasonGroup.addButton(new QRadioButton{i18nc("@option:radio", "Key is superseded"), q},
                              static_cast<int>(RevocationReason::Superseded));
        reasonGroup.addButton(new QRadioButton{i18nc("@option:radio", "Key is no longer used"), q},
                              static_cast<int>(RevocationReason::NoLongerUsed));
        reasonGroup.button(static_cast<int>(RevocationReason::Unspecified))->setChecked(true);

        {
            auto boxLayout = new QVBoxLayout{groupBox};
            for (auto radio : reasonGroup.buttons()) {
                boxLayout->addWidget(radio);
            }
        }

        mainLayout->addWidget(groupBox);
#endif

        {
            auto label = new QLabel{i18nc("@label:textbox", "Description (optional):"), q};
            ui.description = new QTextEdit{q};
            ui.description->setAcceptRichText(false);
            // do not accept Tab as input; this is better for accessibility and
            // tabulators are not really that useful in the description
            ui.description->setTabChangesFocus(true);
            label->setBuddy(ui.description);

            mainLayout->addWidget(label);
            mainLayout->addWidget(ui.description);
        }

        ui.buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        auto okButton = ui.buttonBox->button(QDialogButtonBox::Ok);
        okButton->setText(i18nc("@action:button", "Revoke Key"));
        okButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete-remove")));

        mainLayout->addWidget(ui.buttonBox);

        connect(ui.buttonBox, &QDialogButtonBox::accepted, q, &QDialog::accept);
        connect(ui.buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);

        restoreGeometry();
    }

    ~Private()
    {
        saveGeometry();
    }

private:
    void saveGeometry()
    {
        KConfigGroup cfgGroup(KSharedConfig::openStateConfig(), "RevokeKeyDialog");
        cfgGroup.writeEntry("Size", q->size());
        cfgGroup.sync();
    }

    void restoreGeometry(const QSize &defaultSize = {})
    {
        KConfigGroup cfgGroup(KSharedConfig::openStateConfig(), "RevokeKeyDialog");
        const QSize size = cfgGroup.readEntry("Size", defaultSize);
        if (size.isValid()) {
            q->resize(size);
        }
    }
};

RevokeKeyDialog::RevokeKeyDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog{parent, f}
    , d{new Private{this}}
{
}

RevokeKeyDialog::~RevokeKeyDialog() = default;

void RevokeKeyDialog::setKey(const GpgME::Key &key)
{
    d->key = key;
    d->ui.infoLabel->setText(
        xi18n("<para>You are about to revoke the following key:<nl/>%1</para>")
        .arg(Formatting::summaryLine(key)));
}

#ifdef QGPGME_SUPPORTS_KEY_REVOCATION
GpgME::RevocationReason RevokeKeyDialog::reason() const
{
    return static_cast<RevocationReason>(d->reasonGroup.checkedId());
}
#endif

QString RevokeKeyDialog::description() const
{
    static const QRegularExpression trailingWhitespace{QStringLiteral(R"(\s*$)")};
    return d->ui.description->toPlainText().remove(trailingWhitespace);
}
