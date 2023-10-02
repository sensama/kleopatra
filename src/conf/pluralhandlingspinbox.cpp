/*
    SPDX-FileCopyrightText: 2014 Laurent Montel <montel@kde.org>

    SPDX-License-Identifier: LGPL-2.1-or-later

    This is a copy of KPluralHandlingSpinBox from KTextWidgets.
*/

#include "pluralhandlingspinbox.h"

class PluralHandlingSpinBoxPrivate
{
public:
    PluralHandlingSpinBoxPrivate(QSpinBox *q)
        : q(q)
    {
        QObject::connect(q, &QSpinBox::valueChanged, q, [this](int value) {
            updateSuffix(value);
        });
    }

    void updateSuffix(int value)
    {
        if (!pluralSuffix.isEmpty()) {
            KLocalizedString s = pluralSuffix;
            q->setSuffix(s.subs(value).toString());
        }
    }

    QSpinBox *const q;
    KLocalizedString pluralSuffix;
};

PluralHandlingSpinBox::PluralHandlingSpinBox(QWidget *parent)
    : QSpinBox(parent)
    , d(new PluralHandlingSpinBoxPrivate(this))
{
}

PluralHandlingSpinBox::~PluralHandlingSpinBox() = default;

void PluralHandlingSpinBox::setSuffix(const KLocalizedString &suffix)
{
    d->pluralSuffix = suffix;
    if (suffix.isEmpty()) {
        QSpinBox::setSuffix(QString());
    } else {
        d->updateSuffix(value());
    }
}

#include "moc_pluralhandlingspinbox.cpp"
