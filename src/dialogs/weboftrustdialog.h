/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/weboftrustdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QWidget>

namespace GpgME
{
class Key;
}

namespace Kleo
{
class WebOfTrustWidget;

class WebOfTrustDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WebOfTrustDialog(QWidget *parent = nullptr);
    ~WebOfTrustDialog() override;

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    void fetchMissingKeys();

private:
    QPushButton *mFetchKeysBtn = nullptr;
    WebOfTrustWidget *mWidget = nullptr;
};

} // namespace Kleo
