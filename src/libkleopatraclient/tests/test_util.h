#pragma once

#include <QStringList>
#include <QFileInfo>
#include <QFile>

static QStringList filePathsFromArgs(int argc, char *argv[])
{
    QStringList result;
    for (int i = 1; i < argc; ++i) {
        result.push_back(QFileInfo(QFile::decodeName(argv[i])).absoluteFilePath());
    }
    return result;
}

