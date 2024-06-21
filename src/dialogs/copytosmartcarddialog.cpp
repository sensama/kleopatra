// SPDX-FileCopyrightText: 2024 g10 Code GmbH
// SPDX-FileContributor: Tobias Fella <tobias.fella@gnupg.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "copytosmartcarddialog.h"

#include <Libkleo/Formatting>

#include <KLocalizedString>

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

using namespace Kleo;
using namespace Kleo::Dialogs;

class CopyToSmartcardDialog::Private
{
    friend class ::Kleo::Dialogs::CopyToSmartcardDialog;
    CopyToSmartcardDialog *const q;

public:
    explicit Private(CopyToSmartcardDialog *qq);

private:
    GpgME::Key key;
    QString cardDisplayName;

    struct UI {
        QLabel *label = nullptr;
        QDialogButtonBox *buttonBox = nullptr;
        QRadioButton *deleteRadio = nullptr;
        QRadioButton *fileBackupRadio = nullptr;
        QRadioButton *printBackupRadio = nullptr;
        QRadioButton *existingBackupRadio = nullptr;
        QRadioButton *keepRadio = nullptr;
        QButtonGroup *backupRadios = nullptr;

        QPushButton *acceptButton = nullptr;
    } ui;

    void setUpUI(CopyToSmartcardDialog *q)
    {
        auto layout = new QVBoxLayout;
        q->setLayout(layout);

        ui.label = new QLabel;
        layout->addWidget(ui.label);

        layout->addStretch(1);

        ui.deleteRadio = new QRadioButton(i18nc("@option:radio", "Delete secret key from disk."), q);
        ui.keepRadio = new QRadioButton(i18nc("@option:radio", "Keep secret key on disk."), q);

        auto spacingLayout = new QHBoxLayout;
        spacingLayout->addSpacing(32);
        auto backupLayout = new QVBoxLayout;
        spacingLayout->addLayout(backupLayout);

        ui.fileBackupRadio = new QRadioButton(i18nc("@option:radio", "Make a backup of the secret key to a file."));
        ui.printBackupRadio = new QRadioButton(i18nc("@option:radio", "Make a printed backup of the secret key."));
        ui.existingBackupRadio = new QRadioButton(i18nc("@option:radio", "I already have a backup of the secret key."));

        ui.fileBackupRadio->setEnabled(false);
        ui.printBackupRadio->setEnabled(false);
        ui.existingBackupRadio->setEnabled(false);

        connect(ui.deleteRadio, &QRadioButton::toggled, ui.fileBackupRadio, &QRadioButton::setEnabled);
        connect(ui.deleteRadio, &QRadioButton::toggled, ui.printBackupRadio, &QRadioButton::setEnabled);
        connect(ui.deleteRadio, &QRadioButton::toggled, ui.existingBackupRadio, &QRadioButton::setEnabled);

        connect(ui.deleteRadio, &QRadioButton::toggled, q, [this]() {
            checkAcceptable();
        });
        connect(ui.keepRadio, &QRadioButton::toggled, q, [this]() {
            checkAcceptable();
        });

        ui.backupRadios = new QButtonGroup(q);
        ui.backupRadios->addButton(ui.fileBackupRadio, BackupChoice::FileBackup);
        ui.backupRadios->addButton(ui.printBackupRadio, BackupChoice::PrintBackup);
        ui.backupRadios->addButton(ui.existingBackupRadio, BackupChoice::ExistingBackup);

        connect(ui.backupRadios, &QButtonGroup::buttonToggled, q, [this]() {
            checkAcceptable();
        });

        backupLayout->addWidget(ui.fileBackupRadio);
        backupLayout->addWidget(ui.printBackupRadio);
        backupLayout->addWidget(ui.existingBackupRadio);

        layout->addWidget(ui.deleteRadio);
        layout->addLayout(spacingLayout);
        layout->addWidget(ui.keepRadio);

        ui.buttonBox = new QDialogButtonBox;
        ui.buttonBox->addButton(QDialogButtonBox::Cancel);
        connect(ui.buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);
        ui.acceptButton = ui.buttonBox->addButton(i18nc("@action:button", "Copy to Card"), QDialogButtonBox::AcceptRole);
        connect(ui.buttonBox, &QDialogButtonBox::accepted, q, &QDialog::accept);
        ui.acceptButton->setEnabled(false);
        ui.acceptButton->setIcon(QIcon::fromTheme(QStringLiteral("auth-sim-locked")));
        layout->addWidget(ui.buttonBox);
    }

    void update();
    void checkAcceptable();
};

CopyToSmartcardDialog::Private::Private(CopyToSmartcardDialog *qq)
    : q(qq)
{
    setUpUI(q);
}

CopyToSmartcardDialog::CopyToSmartcardDialog(QWidget *parent)
    : QDialog(parent)
    , d(new Private(this))
{
    setWindowTitle(i18nc("@title:dialog", "Copy Key to Smartcard"));
}

CopyToSmartcardDialog::~CopyToSmartcardDialog() = default;

GpgME::Key CopyToSmartcardDialog::key() const
{
    return d->key;
}

void CopyToSmartcardDialog::setKey(const GpgME::Key &key)
{
    d->key = key;
    d->update();
}

void CopyToSmartcardDialog::Private::update()
{
    ui.label->setText(xi18nc("@info",
                             "<para>Selected Key: <emphasis>%1</emphasis></para><para>Selected Smartcard: <emphasis>%2</emphasis></para><para>Choose one of "
                             "the following options to continue:</para>",
                             Formatting::summaryLine(key),
                             cardDisplayName));
}

QString CopyToSmartcardDialog::cardDisplayName() const
{
    return d->cardDisplayName;
}

void CopyToSmartcardDialog::setCardDisplayName(const QString &cardDisplayName)
{
    d->cardDisplayName = cardDisplayName;
    d->update();
}

void CopyToSmartcardDialog::Private::checkAcceptable()
{
    ui.acceptButton->setEnabled(ui.keepRadio->isChecked() || (ui.deleteRadio->isChecked() && ui.backupRadios->checkedId() != -1));
}

CopyToSmartcardDialog::BackupChoice CopyToSmartcardDialog::backupChoice() const
{
    if (d->ui.keepRadio->isChecked()) {
        return BackupChoice::KeepKey;
    }
    return static_cast<BackupChoice>(d->ui.backupRadios->checkedId());
}

#include "moc_copytosmartcarddialog.cpp"
