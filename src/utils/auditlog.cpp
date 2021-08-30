/* -*- mode: c++; c-basic-offset:4 -*-
    utils/auditlog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "auditlog.h"

#include <QGpgME/Job>

#include <QUrl>
#include <QUrlQuery>
#include "kleopatra_debug.h"
#include <KLocalizedString>

using namespace Kleo;

AuditLog AuditLog::fromJob(const QGpgME::Job *job)
{
    if (job) {
        return AuditLog(job->auditLogAsHtml(), job->auditLogError());
    } else {
        return AuditLog();
    }
}

QUrl AuditLog::asUrl(const QUrl &urlTemplate) const
{
    // more or less the same as
    // kmail/objecttreeparser.cpp:makeShowAuditLogLink(), so any bug
    // fixed here equally applies there:
    if (const int code = m_error.code()) {
        if (code == GPG_ERR_NOT_IMPLEMENTED) {
            qCDebug(KLEOPATRA_LOG) << "not showing link (not implemented)";
        } else if (code == GPG_ERR_NO_DATA) {
            qCDebug(KLEOPATRA_LOG) << "not showing link (not available)";
        } else {
            qCDebug(KLEOPATRA_LOG) << "Error Retrieving Audit Log:" << QString::fromLocal8Bit(m_error.asString());
        }
        return {};
    }

    if (m_text.isEmpty()) {
        return {};
    }

    QUrl url = urlTemplate;
    QUrlQuery urlQuery{url};
    urlQuery.addQueryItem(QStringLiteral("log"), m_text);
    url.setQuery(urlQuery);
    return url;
}
