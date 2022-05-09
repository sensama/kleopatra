/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/checksumsutils_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <Libkleo/ChecksumDefinition>

#include "kleopatra_debug.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#ifdef Q_OS_UNIX
// Can we use QAbstractFileEngine::caseSensitive()?
static const Qt::CaseSensitivity fs_cs = Qt::CaseSensitive;

static const QRegularExpression::PatternOption s_regex_cs = QRegularExpression::NoPatternOption;
#else
static const Qt::CaseSensitivity fs_cs = Qt::CaseInsensitive;
static const QRegularExpression::PatternOption s_regex_cs = QRegularExpression::CaseInsensitiveOption;
#endif

static std::vector<QRegularExpression> get_patterns(const std::vector<std::shared_ptr<Kleo::ChecksumDefinition>> &checksumDefinitions)
{
    std::vector<QRegularExpression> result;
    for (const auto &cd : checksumDefinitions) {
        if (!cd) {
            continue;
        }
        const QStringList &patterns = cd->patterns();
        result.reserve(result.size() + patterns.size());
        std::transform(patterns.cbegin(), patterns.cend(), std::back_inserter(result), [](const QString &pattern) {
            return QRegularExpression(QRegularExpression::anchoredPattern(pattern), s_regex_cs);
        });
    }

    return result;
}

struct matches_any : std::unary_function<QString, bool> {
    const std::vector<QRegularExpression> m_regexps;
    explicit matches_any(const std::vector<QRegularExpression> &regexps) : m_regexps(regexps) {}
    bool operator()(const QString &s) const
    {
        return std::any_of(m_regexps.cbegin(), m_regexps.cend(),
                           [&s](const QRegularExpression &rx) { return rx.match(s).hasMatch(); });
    }
};

struct File {
    QString name;
    QByteArray checksum;
    bool binary;
};

static QString decode(const QString &encoded)
{
    QString decoded;
    decoded.reserve(encoded.size());
    bool shift = false;
    for (const QChar ch : encoded)
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
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }

    QTextStream s(&f);
    static const QRegularExpression rx(QRegularExpression::anchoredPattern(uR"((\\?)([a-f0-9A-F]+) ([ *])([^\\n]+)\\n*)"));
    while (!s.atEnd()) {
        const QString line = s.readLine();
        QRegularExpressionMatch match = rx.match(line);
        if (!match.hasMatch()) {
            continue;
        }

        Q_ASSERT(!match.capturedView(4).endsWith(QLatin1Char('\n')));
        const File file = {
            match.capturedView(1) == QLatin1Char('\\') ? decode(match.captured(4)) : match.captured(4),
            match.capturedView(2).toLatin1(),
            match.capturedView(3) == QLatin1Char('*'),
        };
        files.push_back(file);
    }

    return files;
}

static std::shared_ptr<Kleo::ChecksumDefinition> filename2definition(const QString &fileName,
        const std::vector<std::shared_ptr<Kleo::ChecksumDefinition>> &checksumDefinitions)
{
    auto matchFileName = [&fileName](const std::shared_ptr<Kleo::ChecksumDefinition> &cd) {
        if (!cd) {
            return false;
        }

        const QStringList &patterns = cd->patterns();
        return std::any_of(patterns.cbegin(), patterns.cend(), [&fileName](const QString &pattern) {
            const QRegularExpression re(QRegularExpression::anchoredPattern(pattern), s_regex_cs);
            return re.match(fileName).hasMatch();
        });
    };

    auto it = std::find_if(checksumDefinitions.cbegin(), checksumDefinitions.cend(), matchFileName);

    return it != checksumDefinitions.cend() ? *it : std::shared_ptr<Kleo::ChecksumDefinition>{};
}
