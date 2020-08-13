/*  dialogs/gencardkeydialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gencardkeydialog.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

#include <KEMailSettings>
#include <KEmailAddress>
#include <KLocalizedString>
#include <KColorScheme>

using namespace Kleo;

class GenCardKeyDialog::Private
{
public:
    Private(GenCardKeyDialog *qq): q(qq)
    {
        auto *vBox = new QVBoxLayout(q);
        auto *grid = new QGridLayout;
        vBox->addLayout(grid);

        auto bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, qq);

        mOkButton = bbox->button(QDialogButtonBox::Ok);

        mOkButton->setDefault(true);
        mOkButton->setShortcut(Qt::CTRL | Qt::Key_Return);
        connect(bbox, &QDialogButtonBox::rejected, q, [this]() {q->reject();});
        connect(bbox, &QDialogButtonBox::accepted, q, [this]() {accept();});

        vBox->addWidget(bbox);

        const KEMailSettings e;
        mNameEdit = new QLineEdit(e.getSetting(KEMailSettings::RealName));
        mEmailEdit = new QLineEdit(e.getSetting(KEMailSettings::EmailAddress));

        connect(mEmailEdit, &QLineEdit::textChanged, q, [this]() {checkAcceptable();});

        auto nameLabel = new QLabel(i18n("Name:"));
        auto mailLabel = new QLabel(i18n("EMail:"));
        mInvalidEmailLabel = new QLabel(QStringLiteral("<font size='small' color='%1'>%2</font>").arg(
            KColorScheme(QPalette::Active, KColorScheme::View).foreground(KColorScheme::NegativeText).color().name(), i18n("Invalid EMail")));
        int row = 0;
        grid->addWidget(nameLabel, row, 0);
        grid->addWidget(mNameEdit, row++, 1);
        grid->addWidget(mailLabel, row, 0);
        grid->addWidget(mEmailEdit, row++, 1);
        grid->addWidget(mInvalidEmailLabel, row++, 1);

        // In the future GnuPG may support more algos but for now
        // (2.1.18) we are stuck with RSA for on card generation.
        auto rsaLabel = new QLabel(i18n("RSA Keysize:"));
        mKeySizeCombo = new QComboBox;

        grid->addWidget(rsaLabel, row, 0);
        grid->addWidget(mKeySizeCombo, row++, 1);

        mBackupCheckBox = new QCheckBox(i18n("Backup encryption key"));
        mBackupCheckBox->setToolTip(i18n("Backup the encryption key in a file.") + QStringLiteral("<br/>") +
                                    i18n("You will be asked for a passphrase to protect that file during key generation."));

        mBackupCheckBox->setChecked(true);

        grid->addWidget(mBackupCheckBox, row++, 0, 1, 2);

        q->setMinimumWidth(400);

        checkAcceptable();
    }

    void accept()
    {
        params.name = mNameEdit->text();
        params.email = mEmailEdit->text();
        params.keysize = mKeySizeCombo->currentText().toInt();
        params.algo = GpgME::Subkey::AlgoRSA;
        params.backup = mBackupCheckBox->isChecked();
        q->accept();
    }

    void setSupportedSizes(const std::vector<int> &sizes)
    {
        mKeySizeCombo->clear();
        for (auto size: sizes) {
            mKeySizeCombo->addItem(QString::number(size));
        }
        mKeySizeCombo->setCurrentIndex(mKeySizeCombo->findText(QStringLiteral("2048")));
    }

    void checkAcceptable()
    {
        // We only require a valid mail address
        const QString mail = mEmailEdit->text();
        if (!mail.isEmpty() &&
            KEmailAddress::isValidSimpleAddress(mail)) {
            mOkButton->setEnabled(true);
            mInvalidEmailLabel->hide();
            return;
        }
        if (!mail.isEmpty()) {
            mInvalidEmailLabel->show();
        } else {
            mInvalidEmailLabel->hide();
        }
        mOkButton->setEnabled(false);
    }

    GenCardKeyDialog *const q;
    KeyParams params;
    QPushButton *mOkButton;
    QLineEdit *mNameEdit;
    QLineEdit *mEmailEdit;
    QLabel *mInvalidEmailLabel;
    QComboBox *mKeySizeCombo;
    QCheckBox *mBackupCheckBox;
};

GenCardKeyDialog::GenCardKeyDialog(QWidget *parent) : QDialog(parent),
    d(new Private(this))
{
}

void GenCardKeyDialog::setSupportedSizes(const std::vector<int> &sizes)
{
    d->setSupportedSizes(sizes);
}

GenCardKeyDialog::KeyParams GenCardKeyDialog::getKeyParams() const
{
    return d->params;
}
