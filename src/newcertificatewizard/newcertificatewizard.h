/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/newcertificatewizard.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWizard>

#include <utils/pimpl_ptr.h>

#include <gpgme++/global.h>

namespace Kleo
{

namespace NewCertificateUi
{
class WizardPage;
}

class NewCertificateWizard : public QWizard
{
    Q_OBJECT
public:
    explicit NewCertificateWizard(QWidget *parent = nullptr);
    ~NewCertificateWizard() override;

    void setProtocol(GpgME::Protocol protocol);
    GpgME::Protocol protocol() const;

    enum Pages {
        ChooseProtocolPageId,
        EnterDetailsPageId,
        OverviewPageId,
        KeyCreationPageId,
        ResultPageId,

        NumPages
    };

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    friend class ::Kleo::NewCertificateUi::WizardPage;
};

}

