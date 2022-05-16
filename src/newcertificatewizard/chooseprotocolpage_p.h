/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/chooseprotocolpage_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "wizardpage_p.h"

#include <memory>

class ChooseProtocolPage : public Kleo::NewCertificateUi::WizardPage
{
    Q_OBJECT
public:
    explicit ChooseProtocolPage(QWidget *parent = nullptr);
    ~ChooseProtocolPage() override;

    void setProtocol(GpgME::Protocol proto);
    GpgME::Protocol protocol() const;

    void initializePage() override;

    bool isComplete() const override;

private:
    struct UI;
    std::unique_ptr<UI> ui;

    bool initialized = false;
};
