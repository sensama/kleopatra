/* -*- mode: c++; c-basic-offset:4 -*-
    exportcertificatecommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_EXPORTCERTIFICATECOMMAND_H__
#define __KLEOPATRA_EXPORTCERTIFICATECOMMAND_H__

#include "command.h"

namespace Kleo
{
class ExportCertificateCommand : public Command
{
    Q_OBJECT
public:
    explicit ExportCertificateCommand(QAbstractItemView *view, KeyListController *parent);
    explicit ExportCertificateCommand(KeyListController *parent);
    explicit ExportCertificateCommand(const GpgME::Key &key);
    ~ExportCertificateCommand() override;

    /* reimp */ static Restrictions restrictions()
    {
        return NeedSelection;
    }

    void setOpenPGPFileName(const QString &fileName);
    QString openPGPFileName() const;

    void setX509FileName(const QString &fileName);
    QString x509FileName() const;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void exportResult(GpgME::Error, QByteArray))
};
}

#endif // __KLEOPATRA_EXPORTCERTIFICATECOMMAND_H__

