/* -*- mode: c++; c-basic-offset:4 -*-
    utils/auditlog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <gpgme++/error.h>
#include <gpg-error.h>

class QUrl;

namespace QGpgME
{
class Job;
}

namespace Kleo
{

class AuditLog
{
public:
    AuditLog() : m_text(), m_error() {}
    explicit AuditLog(const GpgME::Error &error)
        : m_text(), m_error(error) {}
    AuditLog(const QString &text, const GpgME::Error &error)
        : m_text(text), m_error(error) {}

    static AuditLog fromJob(const QGpgME::Job *);

    GpgME::Error error() const
    {
        return m_error;
    }
    QString text() const
    {
        return m_text;
    }

    QUrl asUrl(const QUrl &urlTemplate) const;

private:
    QString m_text;
    GpgME::Error m_error;
};

}

