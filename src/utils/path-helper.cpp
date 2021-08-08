/* -*- mode: c++; c-basic-offset:4 -*-
    utils/path-helper.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "path-helper.h"

#include <Libkleo/Stl_Util>

#include <Libkleo/KleoException>

#include "kleopatra_debug.h"
#include <KLocalizedString>

#include <QString>
#include <QStorageInfo>
#include <QFileInfo>
#include <QDir>

#include <algorithm>

using namespace Kleo;

static QString commonPrefix(const QString &s1, const QString &s2)
{
    return QString(s1.data(), std::mismatch(s1.data(), s1.data() + std::min(s1.size(), s2.size()), s2.data()).first - s1.data());
}

static QString longestCommonPrefix(const QStringList &sl)
{
    if (sl.empty()) {
        return QString();
    }
    QString result = sl.front();
    for (const QString &s : sl) {
        result = commonPrefix(s, result);
    }
    return result;
}

QString Kleo::heuristicBaseDirectory(const QStringList &fileNames)
{
    QStringList dirs;
    for (const QString &fileName : fileNames) {
        dirs.push_back(QFileInfo(fileName).path() + QLatin1Char('/'));
    }
    qCDebug(KLEOPATRA_LOG) << "dirs" << dirs;
    const QString candidate = longestCommonPrefix(dirs);
    const int idx = candidate.lastIndexOf(QLatin1Char('/'));
    return candidate.left(idx);
}

QStringList Kleo::makeRelativeTo(const QString &base, const QStringList &fileNames)
{

    if (base.isEmpty()) {
        return fileNames;
    } else {
        return makeRelativeTo(QDir(base), fileNames);
    }

}

QStringList Kleo::makeRelativeTo(const QDir &baseDir, const QStringList &fileNames)
{
    QStringList rv;
    rv.reserve(fileNames.size());
    std::transform(fileNames.cbegin(), fileNames.cend(),
                   std::back_inserter(rv),
                   [&baseDir](const QString &file) {
                       return baseDir.relativeFilePath(file);
                   });
    return rv;
}

void Kleo::recursivelyRemovePath(const QString &path)
{
    const QFileInfo fi(path);
    if (fi.isDir()) {
        QDir dir(path);
        const auto dirs{dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)};
        for (const QString &fname : dirs) {
            recursivelyRemovePath(dir.filePath(fname));
        }
        const QString dirName = fi.fileName();
        dir.cdUp();
        if (!dir.rmdir(dirName)) {
            throw Exception(GPG_ERR_EPERM, i18n("Cannot remove directory %1", path));
        }
    } else {
        QFile file(path);
        if (!file.remove()) {
            throw Exception(GPG_ERR_EPERM, i18n("Cannot remove file %1: %2", path, file.errorString()));
        }
    }
}

bool Kleo::recursivelyCopy(const QString &src,const QString &dest)
{
    QDir srcDir(src);

    if(!srcDir.exists()) {
        return false;
    }

    QDir destDir(dest);
    if(!destDir.exists() && !destDir.mkdir(dest)) {
        return false;
    }

    for(const auto &file: srcDir.entryList(QDir::Files)) {
        const QString srcName = src + QLatin1Char('/') + file;
        const QString destName = dest + QLatin1Char('/') + file;
        if(!QFile::copy(srcName, destName)) {
            return false;
        }
    }

    for (const auto &dir: srcDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
        const QString srcName = src + QLatin1Char('/') + dir;
        const QString destName = dest + QLatin1Char('/') + dir;
        if (!recursivelyCopy(srcName, destName)) {
            return false;
        }
    }

    return true;
}

bool Kleo::moveDir(const QString &src, const QString &dest)
{
    if (QStorageInfo(src).device() == QStorageInfo(dest).device()) {
        // Easy same partition we can use qt.
        return QFile::rename(src, dest);
    }
    // first copy
    if (!recursivelyCopy(src, dest)) {
        return false;
    }
    // Then delete original
    recursivelyRemovePath(src);

    return true;
}
