/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/revokekeydialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "revokekeydialog.h"

#include "utils/accessibility.h"
#include "view/errorlabel.h"

#include <Libkleo/Formatting>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>

#include <QApplication>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFocusEvent>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QTextEdit>
#include <QVBoxLayout>

#include <gpgme++/global.h>
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
        QDialogButtonBox *buttonBox = nullptr;
    } ui;

    Key key;

public:
    Private(RevokeKeyDialog *qq)
        : q(qq)
    {
        q->setWindowTitle(i18nc("title:window", "Revoke Certificate"));

        auto mainLayout = new QVBoxLayout{q};

        ui.infoLabel = new QLabel{q};

        mainLayout->addWidget(ui.infoLabel);

        ui.buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        auto okButton = ui.buttonBox->button(QDialogButtonBox::Ok);
        okButton->setText(i18nc("@action:button", "Revoke Certificate"));
        okButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete-remove")));

        mainLayout->addWidget(ui.buttonBox);

        connect(ui.buttonBox, &QDialogButtonBox::accepted, q, [this]() {
            checkAccept();
        });
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
        KConfigGroup cfgGroup(KSharedConfig::openStateConfig(), QStringLiteral("RevokeKeyDialog"));
        cfgGroup.writeEntry("Size", q->size());
        cfgGroup.sync();
    }

    void restoreGeometry(const QSize &defaultSize = {})
    {
        KConfigGroup cfgGroup(KSharedConfig::openStateConfig(), QStringLiteral("RevokeKeyDialog"));
        const QSize size = cfgGroup.readEntry("Size", defaultSize);
        if (size.isValid()) {
            q->resize(size);
        } else {
            q->resize(q->minimumSizeHint());
        }
    }

    void checkAccept()
    {
        q->accept();
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
        xi18nc("@info",
               "<para>You are about to revoke the following certificate:</para><para>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;%1</para><para><emphasis "
               "strong='true'>The "
               "revocation will take effect "
               "immediately and "
               "cannot be reverted.</emphasis></para><para>Consequences: <list><item>You cannot sign anything anymore with this certificate.</item><item>You "
               "can still decrypt everything encrypted for this certificate.</item><item>Other people can no longer encrypt for this certificate after "
               "receiving the revocation.</item><item>You cannot certify other certificates anymore with this certificate.</item></list></para>")
            .arg(Formatting::summaryLine(key)));
}

#include "moc_revokekeydialog.cpp"
