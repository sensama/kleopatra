/*  utils/accessibility.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "accessibility.h"

#include <KLocalizedString>

#include <QAction>
#ifdef Q_OS_WIN
#include <QApplication>
#include <QDesktopWidget>
#endif
#include <QLabel>
#include <QTextDocument>
#include <QToolTip>

#include <algorithm>
#include <chrono>

#include "kleopatra_debug.h"

using namespace Kleo;

static const char *accessibleNameProperty = "_kleo_accessibleName";
static const char *accessibleValueProperty = "_kleo_accessibleValue";

static const char *useAccessibleValueLabelProperty = "_kleo_useAccessibleValueLabel";

namespace
{
QString getAccessibleText(QWidget *widget, QAccessible::Text t)
{
    QString name;
    if (const auto *const iface = QAccessible::queryAccessibleInterface(widget)) {
        name = iface->text(t);
    }
    return name;
}
}

QString Kleo::getAccessibleName(QWidget *widget)
{
    return getAccessibleText(widget, QAccessible::Name);
}

QString Kleo::getAccessibleDescription(QWidget *widget)
{
    return getAccessibleText(widget, QAccessible::Description);
}

void Kleo::setAccessibleName(QAction *action, const QString &name)
{
    action->setProperty(accessibleNameProperty, name);
}

QString Kleo::getAccessibleName(const QAction *action)
{
    return action->property(accessibleNameProperty).toString();
}

void Kleo::setAccessibleValue(QWidget *widget, const QString &value)
{
    widget->setProperty(accessibleValueProperty, value);
}

QString Kleo::getAccessibleValue(const QWidget *widget)
{
    return widget->property(accessibleValueProperty).toString();
}

void Kleo::setRepresentAsAccessibleValueWidget(QWidget *widget, bool flag)
{
    widget->setProperty(useAccessibleValueLabelProperty, flag ? flag : QVariant{});
}

bool Kleo::representAsAccessibleValueWidget(const QWidget *widget)
{
    return widget->property(useAccessibleValueLabelProperty).toBool();
}

QString Kleo::invalidEntryText()
{
    return i18nc(
        "text for screen readers to indicate that the associated object, "
        "such as a form field, has an error",
        "invalid entry");
}

QString Kleo::requiredText()
{
    return i18nc(
        "text for screen readers to indicate that the associated object, "
        "such as a form field must be filled out",
        "required");
}

void Kleo::selectLabelText(QLabel *label)
{
    if (!label || label->text().isEmpty()) {
        return;
    }
    if (label->textFormat() == Qt::PlainText) {
        label->setSelection(0, label->text().size());
    } else if (label->textFormat() == Qt::RichText) {
        // unfortunately, there is no selectAll(); therefore, we need
        // to determine the "visual" length of the text by stripping
        // the label's text of all formatting information
        QTextDocument temp;
        temp.setHtml(label->text());
        label->setSelection(0, temp.toRawText().size());
    } else {
        qCDebug(KLEOPATRA_LOG) << "Label with unsupported text format" << label->textFormat() << "got focus";
    }
}

namespace
{
static void notifyAccessibilityClientsAboutToolTip(const QPoint &pos, QWidget *parent)
{
#ifdef Q_OS_WIN
    // On Windows, the tool tip's parent widget is a desktop screen widget (see implementation of QToolTip::showText)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    const auto desktop = QApplication::desktop();
    const int screenNumber = desktop->isVirtualDesktop() ? desktop->screenNumber(pos) : desktop->screenNumber(parent);
    parent = desktop->screen(screenNumber);
    QT_WARNING_POP
#else
    Q_UNUSED(pos);
#endif
    if (!parent) {
        return;
    }
    if (auto toolTipLabel = parent->findChild<QLabel *>(QStringLiteral("qtooltip_label"))) {
        // Qt explicitly does not notify accessibility clients about the tool tip being shown because
        // "Tooltips are read aloud twice in MS narrator."
        // The problem is that they are not read out by orca (on Linux) if the notification is omitted.
        // Therefore, we take care of notifying the accessibility clients.
#ifndef QT_NO_ACCESSIBILITY
        QAccessibleEvent event(toolTipLabel, QAccessible::ObjectShow);
        QAccessible::updateAccessibility(&event);
#endif
    }
}
}

void Kleo::showToolTip(const QPoint &pos, const QString &text, QWidget *w)
{
    using namespace std::chrono_literals;
    static const std::chrono::milliseconds timeout = 24h;
    QToolTip::showText(pos, text, w, {}, timeout.count());
    notifyAccessibilityClientsAboutToolTip(pos, w);
}

LabelHelper::LabelHelper()
{
    QAccessible::installActivationObserver(this);
}

LabelHelper::~LabelHelper()
{
    QAccessible::removeActivationObserver(this);
}

void LabelHelper::addLabel(QLabel *label)
{
    mLabels.push_back(label);
    accessibilityActiveChanged(QAccessible::isActive());
}

void LabelHelper::accessibilityActiveChanged(bool active)
{
    // Allow text labels to get focus if accessibility is active
    const auto focusPolicy = active ? Qt::StrongFocus : Qt::ClickFocus;
    std::for_each(std::cbegin(mLabels), std::cend(mLabels), [focusPolicy](const auto &label) {
        if (label) {
            label->setFocusPolicy(focusPolicy);
        }
    });
}
