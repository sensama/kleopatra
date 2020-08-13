/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/exportcertificatesdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "exportcertificatesdialog.h"

#include <Libkleo/FileNameRequester>

#include <KGuiItem>
#include <KLocalizedString>

#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>

using namespace Kleo;
using namespace Kleo::Dialogs;

class ExportCertificatesDialog::Private
{
    friend class ::Kleo::Dialogs::ExportCertificatesDialog;
    ExportCertificatesDialog *const q;
public:
    explicit Private(ExportCertificatesDialog *qq);
    ~Private();
    void fileNamesChanged();

private:
    FileNameRequester *pgpRequester;
    FileNameRequester *cmsRequester;
    QPushButton *mOkButton;
};

ExportCertificatesDialog::Private::Private(ExportCertificatesDialog *qq)
    : q(qq)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(q);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, qq);
    mOkButton = buttonBox->button(QDialogButtonBox::Ok);
    mOkButton->setDefault(true);
    mOkButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    connect(buttonBox, &QDialogButtonBox::accepted, q, &ExportCertificatesDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, q, &ExportCertificatesDialog::reject);

    KGuiItem::assign(mOkButton, KGuiItem(i18n("Export")));
    QWidget *const main = new QWidget;
    mainLayout->addWidget(main);
    mainLayout->addWidget(buttonBox);

    QFormLayout *layout = new QFormLayout;
    main->setLayout(layout);

    QLabel *const pgpLabel = new QLabel;
    pgpLabel->setText(i18n(" OpenPGP export file:"));
    pgpRequester = new FileNameRequester;
    pgpRequester->setExistingOnly(false);
    connect(pgpRequester, SIGNAL(fileNameChanged(QString)), q, SLOT(fileNamesChanged()));
    layout->addRow(pgpLabel, pgpRequester);

    QLabel *const cmsLabel = new QLabel;
    cmsLabel->setText(i18n("S/MIME export file:"));
    cmsRequester = new FileNameRequester;
    cmsRequester->setExistingOnly(false);
    layout->addRow(cmsLabel, cmsRequester);

    connect(cmsRequester, SIGNAL(fileNameChanged(QString)), q, SLOT(fileNamesChanged()));
    fileNamesChanged();
}

ExportCertificatesDialog::Private::~Private() {}

ExportCertificatesDialog::ExportCertificatesDialog(QWidget *parent)
    : QDialog(parent), d(new Private(this))
{

}

void ExportCertificatesDialog::Private::fileNamesChanged()
{
    mOkButton->setEnabled(!pgpRequester->fileName().isEmpty() && !cmsRequester->fileName().isEmpty());
}

ExportCertificatesDialog::~ExportCertificatesDialog() {}

void ExportCertificatesDialog::setOpenPgpExportFileName(const QString &fileName)
{
    d->pgpRequester->setFileName(fileName);
}

QString ExportCertificatesDialog::openPgpExportFileName() const
{
    return d->pgpRequester->fileName();
}

void ExportCertificatesDialog::setCmsExportFileName(const QString &fileName)
{
    d->cmsRequester->setFileName(fileName);
}

QString ExportCertificatesDialog::cmsExportFileName() const
{
    return d->cmsRequester->fileName();
}

#include "moc_exportcertificatesdialog.cpp"

