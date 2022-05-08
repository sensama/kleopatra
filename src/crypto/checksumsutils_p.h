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

static QList<QRegularExpression> get_patterns(const std::vector< std::shared_ptr<Kleo::ChecksumDefinition> > &checksumDefinitions)
{
    QList<QRegularExpression> result;
    for (const std::shared_ptr<Kleo::ChecksumDefinition> &cd : checksumDefinitions)
        if (cd) {
            const auto patterns = cd->patterns();
            for (const QString &pattern : patterns) {
                result.push_back(QRegularExpression(QRegularExpression::anchoredPattern(pattern), s_regex_cs));
            }
        }
    return result;
}

struct matches_any : std::unary_function<QString, bool> {
    const QList<QRegularExpression> m_regexps;
    explicit matches_any(const QList<QRegularExpression> &regexps) : m_regexps(regexps) {}
    bool operator()(const QString &s) const
    {
        return std::any_of(m_regexps.cbegin(), m_regexps.cend(),
                           [s](const QRegularExpression &rx) { return rx.match(s).hasMatch(); });
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
        static const QRegularExpression rx(QRegularExpression::anchoredPattern(uR"((\\?)([a-f0-9A-F]+) ([ *])([^\\n]+)\\n*)"));
        while (!s.atEnd()) {
            const QString line = s.readLine();
            QRegularExpressionMatch match = rx.match(line);
            if (match.hasMatch()) {
                Q_ASSERT(!match.capturedView(4).endsWith(QLatin1Char('\n')));
                const File file = {
                    match.capturedView(1) == QLatin1String("\\") ? decode(match.captured(4)) : match.captured(4),
                    match.capturedView(2).toLatin1(),
                    match.capturedView(3) == QLatin1String("*"),
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
                const QRegularExpression re(QRegularExpression::anchoredPattern(pattern), s_regex_cs);
                if (re.match(fileName).hasMatch()) {
                    return cd;
                }
            }
        }
    }
    return std::shared_ptr<Kleo::ChecksumDefinition>();
}
