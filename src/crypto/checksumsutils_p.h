/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/checksumsutils_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QFile>

#include <Libkleo/ChecksumDefinition>
#include "kleopatra_debug.h"

#ifdef Q_OS_UNIX
// can we use QAbstractFileEngine::caseSensitive()?
static const Qt::CaseSensitivity fs_cs = Qt::CaseSensitive;
#else
static const Qt::CaseSensitivity fs_cs = Qt::CaseInsensitive;
#endif

static QList<QRegExp> get_patterns(const std::vector< std::shared_ptr<Kleo::ChecksumDefinition> > &checksumDefinitions)
{
    QList<QRegExp> result;
    for (const std::shared_ptr<Kleo::ChecksumDefinition> &cd : checksumDefinitions)
        if (cd) {
            const auto patterns = cd->patterns();
            for (const QString &pattern : patterns) {
                result.push_back(QRegExp(pattern, fs_cs));
            }
        }
    return result;
}

namespace
{

struct matches_any : std::unary_function<QString, bool> {
    const QList<QRegExp> m_regexps;
    explicit matches_any(const QList<QRegExp> &regexps) : m_regexps(regexps) {}
    bool operator()(const QString &s) const
    {
        return std::any_of(m_regexps.cbegin(), m_regexps.cend(),
                           [s](const QRegExp &rx) { return rx.exactMatch(s); });
    }
};
}

namespace
{
struct File {
    QString name;
    QByteArray checksum;
    bool binary;
};
}

static QString decode(const QString &encoded)
{
    QString decoded;
    decoded.reserve(encoded.size());
    bool shift = false;
    for (QChar ch : encoded)
        if (shift) {
            switch (ch.toLatin1()) {
            case '\\': decoded += QLatin1Char('\\'); break;
            case 'n':  decoded += QLatin1Char('\n'); break;
            default:
                qCDebug(KLEOPATRA_LOG) << "invalid escape sequence" << '\\' << ch << "(interpreted as '" << ch << "')";
                decoded += ch;
                break;
            }
            shift = false;
        } else {
            if (ch == QLatin1Char('\\')) {
                shift = true;
            } else {
                decoded += ch;
            }
        }
    return decoded;
}

static std::vector<File> parse_sum_file(const QString &fileName)
{
    std::vector<File> files;
    QFile f(fileName);
    if (f.open(QIODevice::ReadOnly)) {
        QTextStream s(&f);
        QRegExp rx(QLatin1String("(\\?)([a-f0-9A-F]+) ([ *])([^\n]+)\n*"));
        while (!s.atEnd()) {
            const QString line = s.readLine();
            if (rx.exactMatch(line)) {
                Q_ASSERT(!rx.cap(4).endsWith(QLatin1Char('\n')));
                const File file = {
                    rx.cap(1) == QLatin1String("\\") ? decode(rx.cap(4)) : rx.cap(4),
                    rx.cap(2).toLatin1(),
                    rx.cap(3) == QLatin1String("*"),
                };
                files.push_back(file);
            }
        }
    }
    return files;
}


static std::shared_ptr<Kleo::ChecksumDefinition> filename2definition(const QString &fileName,
        const std::vector< std::shared_ptr<Kleo::ChecksumDefinition> > &checksumDefinitions)
{
    for (const std::shared_ptr<Kleo::ChecksumDefinition> &cd : checksumDefinitions) {
        if (cd) {
            const auto patterns = cd->patterns();
            for (const QString &pattern : patterns) {
                if (QRegExp(pattern, fs_cs).exactMatch(fileName)) {
                    return cd;
                }
            }
        }
    }
    return std::shared_ptr<Kleo::ChecksumDefinition>();
}
