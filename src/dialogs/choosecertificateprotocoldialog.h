/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/choosecertificateprotocoldialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

#include <gpgme++/global.h>

#include <memory>

namespace Kleo
{

class ChooseCertificateProtocolDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ChooseCertificateProtocolDialog(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~ChooseCertificateProtocolDialog() override;

    GpgME::Protocol protocol() const;

protected:
    void showEvent(QShowEvent *event) override;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

}
