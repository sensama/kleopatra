/*  utils/accessibility.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "accessibility.h"

#include <KLocalizedString>

#include <QAccessible>
#include <QObject>

QString Kleo::getAccessibleName(QObject *object)
{
    QString name;
    if (const auto *const iface = QAccessible::queryAccessibleInterface(object)) {
        name = iface->text(QAccessible::Name);
    }
    return name;
}

QString Kleo::invalidEntryText()
{
    return i18nc("text for screen readers to indicate that the associated object, "
                 "such as a form field, has an error",
                 "invalid entry");
}
