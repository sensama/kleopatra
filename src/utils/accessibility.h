/*  utils/accessibility.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QAccessible>
#include <QPointer>

class QLabel;
class QObject;
class QString;

namespace Kleo
{
    QString getAccessibleName(QObject *object);
    QString getAccessibleDescription(QObject *object);
    QString invalidEntryText();
    QString requiredText();

    /**
     * Simple helper that sets the focus policy of the associated labels
     * to \c Qt::StrongFocus if an assistive tool is active.
     */
    class LabelHelper: public QAccessible::ActivationObserver
    {
    public:
        LabelHelper();
        ~LabelHelper() override;
        Q_DISABLE_COPY_MOVE(LabelHelper)

        void addLabel(QLabel *label);

    private:
        void accessibilityActiveChanged(bool active) override;

        std::vector<QPointer<QLabel>> mLabels;
    };
}
