/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/lookupcertificatesdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_DIALOGS_LOOKUPCERTIFICATESDIALOG_H__
#define __KLEOPATRA_DIALOGS_LOOKUPCERTIFICATESDIALOG_H__

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

class LookupCertificatesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LookupCertificatesDialog(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~LookupCertificatesDialog() override;

    void setCertificates(const std::vector<GpgME::Key> &certs);
    std::vector<GpgME::Key> selectedCertificates() const;

    void setPassive(bool passive);
    bool isPassive() const;
    void setSearchText(const QString &text);
    QString searchText() const;

Q_SIGNALS:
    void searchTextChanged(const QString &text);
    void saveAsRequested(const std::vector<GpgME::Key> &certs);
    void importRequested(const std::vector<GpgME::Key> &certs);
    void detailsRequested(const GpgME::Key &certs);

public Q_SLOTS:
    void accept() override;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotSearchTextChanged())
    Q_PRIVATE_SLOT(d, void slotSearchClicked())
    Q_PRIVATE_SLOT(d, void slotSelectionChanged())
    Q_PRIVATE_SLOT(d, void slotDetailsClicked())
    Q_PRIVATE_SLOT(d, void slotSaveAsClicked())
};

}
}

#endif /* __KLEOPATRA_DIALOGS_LOOKUPCERTIFICATESDIALOG_H__ */
