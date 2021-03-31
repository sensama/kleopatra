/* -*- mode: c++; c-basic-offset:4 -*-
    utils/path-helper.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

class QString;
#include <QStringList>
class QDir;

namespace Kleo
{

QString heuristicBaseDirectory(const QStringList &files);
QStringList makeRelativeTo(const QDir &dir, const QStringList &files);
QStringList makeRelativeTo(const QString &dir, const QStringList &files);

void recursivelyRemovePath(const QString &path);
bool recursivelyCopy(const QString &src, const QString &dest);
bool moveDir(const QString &src, const QString &dest);
}

