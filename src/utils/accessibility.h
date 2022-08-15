/*  utils/accessibility.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QAccessible>
#include <QPointer>

class QAction;
class QLabel;
class QObject;
class QString;

namespace Kleo
{
    QString getAccessibleName(QWidget *widget);
    QString getAccessibleDescription(QWidget *widget);

    /**
     * Sets the accessible name of the action \p action.
     *
     * \note Qt does not provide an accessible object for a QAction. Therefore,
     *       we store the accessible name as custom property of the action.
     * \sa getAccessibleName
     */
    void setAccessibleName(QAction *action, const QString &name);
    /**
     * Returns the accessible name of the action \p action.
     * \sa setAccessibleName
     */
    QString getAccessibleName(const QAction *action);

    QString invalidEntryText();
    QString requiredText();

    /**
     * Selects the text displayed by the label. Only \ref QLabel with text format
     * \c Qt::PlainText or \c Qt::RichText are supported.
     */
    void selectLabelText(QLabel *label);

    /**
     * Shows \p text as a tool tip, with the global position \p pos as the point of interest.
     * Additionally to QToolTip::showText, it takes care of notifying accessibility clients
     * about the tool tip.
     * \sa QToolTip::showText
     */
    void showToolTip(const QPoint &pos, const QString &text, QWidget *w);

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
