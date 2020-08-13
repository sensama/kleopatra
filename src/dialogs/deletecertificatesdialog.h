/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/deletecertificatesdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_DIALOGS_DELETECERTIFICATESDIALOG_H__
#define __KLEOPATRA_DIALOGS_DELETECERTIFICATESDIALOG_H__

#include <QDialog>

#include <utils/pimpl_ptr.h>

#include <vector>

namespace GpgME
{
class Key;
}

namespace Kleo
{
namespace Dialogs
{

class DeleteCertificatesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DeleteCertificatesDialog(QWidget *parent = nullptr);
    ~DeleteCertificatesDialog() override;

    void setSelectedKeys(const std::vector<GpgME::Key> &keys);
    void setUnselectedKeys(const std::vector<GpgME::Key> &keys);

    std::vector<GpgME::Key> keys() const;

    void accept() override;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotWhatsThisRequested())
};

}
}

#endif /* __KLEOPATRA_DIALOGS_DELETECERTIFICATESDIALOG_H__ */
