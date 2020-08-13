/* -*- mode: c++; c-basic-offset:4 -*-
    conf/smimevalidationconfigurationpage.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KLEOPATRA_SMIMEVALIDATIONCONFIGURATIONPAGE_H
#define KLEOPATRA_SMIMEVALIDATIONCONFIGURATIONPAGE_H

#include <KCModule>
namespace Kleo
{
namespace Config
{

class SMimeValidationConfigurationWidget;

class SMimeValidationConfigurationPage : public KCModule
{
    Q_OBJECT
public:
    explicit SMimeValidationConfigurationPage(QWidget *parent = nullptr, const QVariantList &args = QVariantList());

    void load() override;
    void save() override;
    void defaults() override;

private:
    SMimeValidationConfigurationWidget *mWidget;
};

}
}

#endif // KLEOPATRA_SMIMEVALIDATIONCONFIGURATIONPAGE_H
