/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/exportcertificatesdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_DIALOGS_EXPORTCERTIFICATESDIALOG_H__
#define __KLEOPATRA_DIALOGS_EXPORTCERTIFICATESDIALOG_H__

#include <QDialog>

#include <utils/pimpl_ptr.h>

class QString;

namespace Kleo
{
namespace Dialogs
{

class ExportCertificatesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportCertificatesDialog(QWidget *parent = nullptr);
    ~ExportCertificatesDialog();

    void setOpenPgpExportFileName(const QString &fileName);
    QString openPgpExportFileName() const;

    void setCmsExportFileName(const QString &fileName);
    QString cmsExportFileName() const;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void fileNamesChanged())
};
}
}

#endif // __KLEOPATRA_DIALOGS_EXPORTCERTIFICATESDIALOG_H__
