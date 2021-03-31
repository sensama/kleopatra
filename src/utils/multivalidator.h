/* -*- mode: c++; c-basic-offset:4 -*-
    utils/multivalidator.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QValidator>
#include <QList>

namespace Kleo
{

class MultiValidator : public QValidator
{
    Q_OBJECT
public:
    explicit MultiValidator(QObject *parent = nullptr)
        : QValidator(parent) {}
    explicit MultiValidator(QValidator *validator1, QValidator *validator2 = nullptr, QObject *parent = nullptr)
        : QValidator(parent)
    {
        addValidator(validator1);
        addValidator(validator2);
    }
    explicit MultiValidator(const QList<QValidator *> &validators, QObject *parent = nullptr)
        : QValidator(parent)
    {
        addValidators(validators);
    }
    ~MultiValidator() override;

    void addValidator(QValidator *validator);
    void addValidators(const QList<QValidator *> &validators);

    void removeValidator(QValidator *validator);
    void removeValidators(const QList<QValidator *> &validators);

    void fixup(QString &str) const override;
    State validate(QString &str, int &pos) const override;

private Q_SLOTS:
    void _kdmv_slotDestroyed(QObject *);

private:
    QList<QValidator *> m_validators;
};

}

