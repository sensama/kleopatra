/*  utils/accessibility.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

class QObject;
class QString;

namespace Kleo
{
    QString getAccessibleName(QObject *object);
    QString getAccessibleDescription(QObject *object);
    QString invalidEntryText();
}
