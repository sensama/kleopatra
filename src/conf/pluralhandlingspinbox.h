/*
    SPDX-FileCopyrightText: 2014 Laurent Montel <montel@kde.org>

    SPDX-License-Identifier: LGPL-2.1-or-later

    This is a copy of KPluralHandlingSpinBox from KTextWidgets.
*/

#pragma once

#include <KLocalizedString>

#include <QSpinBox>

#include <memory>

class PluralHandlingSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    /**
     * Default constructor
     */

    explicit PluralHandlingSpinBox(QWidget *parent = nullptr);
    ~PluralHandlingSpinBox() override;

    /**
     * Sets the suffix to @p suffix.
     * Use this to add a plural-aware suffix, e.g. by using ki18np("singular", "plural").
     */
    void setSuffix(const KLocalizedString &suffix);

private:
    friend class PluralHandlingSpinBoxPrivate;
    std::unique_ptr<class PluralHandlingSpinBoxPrivate> const d;

    Q_DISABLE_COPY(PluralHandlingSpinBox)
};
