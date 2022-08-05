/*  utils/applicationstate.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "applicationstate.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QFileInfo>
#include <QStandardPaths>

QString ApplicationState::lastUsedExportDirectory()
{
    const KConfigGroup config{KSharedConfig::openConfig(), "ExportDialog"};
    return config.readEntry("LastDirectory", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
}

void ApplicationState::setLastUsedExportDirectory(const QString &path)
{
    KConfigGroup config{KSharedConfig::openConfig(), "ExportDialog"};
    config.writeEntry("LastDirectory", QFileInfo{path}.absolutePath());
}
