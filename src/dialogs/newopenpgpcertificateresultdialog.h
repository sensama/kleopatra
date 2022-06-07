/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/newopenpgpcertificateresultdialog.h

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

#include <memory>

namespace GpgME
{
class Key;
class KeyGenerationResult;
}

namespace Kleo
{
class KeyParameters;

class NewOpenPGPCertificateResultDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewOpenPGPCertificateResultDialog(const GpgME::KeyGenerationResult &result,
                                               const GpgME::Key &key,
                                               QWidget *parent = nullptr,
                                               Qt::WindowFlags f = {});
    ~NewOpenPGPCertificateResultDialog() override;

Q_SIGNALS:
    void retry();

private:
    class Private;
    const std::unique_ptr<Private> d;
};

}
