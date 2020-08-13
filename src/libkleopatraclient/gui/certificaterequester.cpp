/* -*- mode: c++; c-basic-offset:4 -*-
    gui/certificaterequester.h

    This file is part of KleopatraClient, the Kleopatra interface library
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "certificaterequester.h"

#include <libkleopatraclient/core/selectcertificatecommand.h>

#include <QPointer>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include <QHBoxLayout>
#include <KLocalizedString>

#include <memory>

using namespace KleopatraClientCopy;
using namespace KleopatraClientCopy::Gui;

class CertificateRequester::Private
{
    friend class ::KleopatraClientCopy::Gui::CertificateRequester;
    CertificateRequester *const q;
public:
    explicit Private(CertificateRequester *qq)
        : q(qq),
          selectedCertificates(),
          command(),
          multipleCertificatesAllowed(false),
          onlySigningCertificatesAllowed(false),
          onlyEncryptionCertificatesAllowed(false),
          onlyOpenPGPCertificatesAllowed(false),
          onlyX509CertificatesAllowed(false),
          onlySecretKeysAllowed(false),
          ui(q)
    {

    }

private:
    void updateLineEdit()
    {
        ui.lineEdit.setText(selectedCertificates.join(QLatin1Char(' ')));
    }
    void createCommand()
    {
        std::unique_ptr<SelectCertificateCommand> cmd(new SelectCertificateCommand);

        cmd->setMultipleCertificatesAllowed(multipleCertificatesAllowed);
        cmd->setOnlySigningCertificatesAllowed(onlySigningCertificatesAllowed);
        cmd->setOnlyEncryptionCertificatesAllowed(onlyEncryptionCertificatesAllowed);
        cmd->setOnlyOpenPGPCertificatesAllowed(onlyOpenPGPCertificatesAllowed);
        cmd->setOnlyX509CertificatesAllowed(onlyX509CertificatesAllowed);
        cmd->setOnlySecretKeysAllowed(onlySecretKeysAllowed);

        cmd->setSelectedCertificates(selectedCertificates);

        if (const QWidget *const window = q->window()) {
            cmd->setParentWId(window->effectiveWinId());
        }

        connect(cmd.get(), SIGNAL(finished()), q, SLOT(slotCommandFinished()));

        command = cmd.release();
    }

    void slotButtonClicked();
    void slotCommandFinished();

private:
    QStringList selectedCertificates;

    QPointer<SelectCertificateCommand> command;

    bool multipleCertificatesAllowed : 1;
    bool onlySigningCertificatesAllowed : 1;
    bool onlyEncryptionCertificatesAllowed : 1;
    bool onlyOpenPGPCertificatesAllowed : 1;
    bool onlyX509CertificatesAllowed : 1;
    bool onlySecretKeysAllowed : 1;

    struct Ui {
        QLineEdit lineEdit;
        QPushButton button;
        QHBoxLayout hlay;

        explicit Ui(CertificateRequester *qq)
            : lineEdit(qq),
              button(i18n("Change..."), qq),
              hlay(qq)
        {
            lineEdit.setObjectName(QStringLiteral("lineEdit"));
            button.setObjectName(QStringLiteral("button"));
            hlay.setObjectName(QStringLiteral("hlay"));

            hlay.addWidget(&lineEdit, 1);
            hlay.addWidget(&button);

            lineEdit.setReadOnly(true);

            connect(&button, SIGNAL(clicked()),
                    qq, SLOT(slotButtonClicked()));
        }

    } ui;
};

CertificateRequester::CertificateRequester(QWidget *p, Qt::WindowFlags f)
    : QWidget(p, f), d(new Private(this))
{

}

CertificateRequester::~CertificateRequester()
{
    delete d; d = nullptr;
}

void CertificateRequester::setMultipleCertificatesAllowed(bool allow)
{
    if (allow == d->multipleCertificatesAllowed) {
        return;
    }
    d->multipleCertificatesAllowed = allow;
}

bool CertificateRequester::multipleCertificatesAllowed() const
{
    return d->multipleCertificatesAllowed;
}

void CertificateRequester::setOnlySigningCertificatesAllowed(bool allow)
{
    if (allow == d->onlySigningCertificatesAllowed) {
        return;
    }
    d->onlySigningCertificatesAllowed = allow;
}

bool CertificateRequester::onlySigningCertificatesAllowed() const
{
    return d->onlySigningCertificatesAllowed;
}

void CertificateRequester::setOnlyEncryptionCertificatesAllowed(bool allow)
{
    if (allow == d->onlyEncryptionCertificatesAllowed) {
        return;
    }
    d->onlyEncryptionCertificatesAllowed = allow;
}

bool CertificateRequester::onlyEncryptionCertificatesAllowed() const
{
    return d->onlyEncryptionCertificatesAllowed;
}

void CertificateRequester::setOnlyOpenPGPCertificatesAllowed(bool allow)
{
    if (allow == d->onlyOpenPGPCertificatesAllowed) {
        return;
    }
    d->onlyOpenPGPCertificatesAllowed = allow;
}

bool CertificateRequester::onlyOpenPGPCertificatesAllowed() const
{
    return d->onlyOpenPGPCertificatesAllowed;
}

void CertificateRequester::setOnlyX509CertificatesAllowed(bool allow)
{
    if (allow == d->onlyX509CertificatesAllowed) {
        return;
    }
    d->onlyX509CertificatesAllowed = allow;
}

bool CertificateRequester::onlyX509CertificatesAllowed() const
{
    return d->onlyX509CertificatesAllowed;
}

void CertificateRequester::setOnlySecretKeysAllowed(bool allow)
{
    if (allow == d->onlySecretKeysAllowed) {
        return;
    }
    d->onlySecretKeysAllowed = allow;
}

bool CertificateRequester::onlySecretKeysAllowed() const
{
    return d->onlySecretKeysAllowed;
}

void CertificateRequester::setSelectedCertificates(const QStringList &certs)
{
    if (certs == d->selectedCertificates) {
        return;
    }
    d->selectedCertificates = certs;
    d->updateLineEdit();
    Q_EMIT selectedCertificatesChanged(certs);
}

QStringList CertificateRequester::selectedCertificates() const
{
    return d->selectedCertificates;
}

void CertificateRequester::setSelectedCertificate(const QString &cert)
{
    setSelectedCertificates(QStringList(cert));
}

QString CertificateRequester::selectedCertificate() const
{
    return d->selectedCertificates.empty() ? QString() : d->selectedCertificates.front();
}

void CertificateRequester::Private::slotButtonClicked()
{
    if (command) {
        return;
    }
    createCommand();
    command->start();
    ui.button.setEnabled(false);
}

void CertificateRequester::Private::slotCommandFinished()
{
    if (command->wasCanceled()) {
        /* do nothing */;
    } else if (command->error()) {
        QMessageBox::information(q,
                                 i18n("Kleopatra Error"),
                                 i18n("There was an error while connecting to Kleopatra: %1",
                                      command->errorString()));
    } else {
        q->setSelectedCertificates(command->selectedCertificates());
    }
    ui.button.setEnabled(true);
    delete command;
}

#include "moc_certificaterequester.cpp"
