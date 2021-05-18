/*  view/openpgpkeycardwidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QWidget>

#include <memory>

namespace Kleo
{

namespace SmartCard
{
class Card;
}

class OpenPGPKeyCardWidget: public QWidget
{
    Q_OBJECT
public:
    explicit OpenPGPKeyCardWidget(QWidget *parent = nullptr);
    ~OpenPGPKeyCardWidget() override;

public Q_SLOTS:
    void update(const SmartCard::Card *card);

Q_SIGNALS:
    void createCSRRequested(const std::string &keyRef);

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
