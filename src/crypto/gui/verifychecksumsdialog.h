/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/verifychecksumsdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_CRYPTO_GUI_VERIFYCHECKSUMSDIALOG_H__
#define __KLEOPATRA_CRYPTO_GUI_VERIFYCHECKSUMSDIALOG_H__

#include <QDialog>
#include <QMetaType>

#ifndef QT_NO_DIRMODEL

#include <utils/pimpl_ptr.h>

namespace Kleo
{
namespace Crypto
{
namespace Gui
{

class VerifyChecksumsDialog : public QDialog
{
    Q_OBJECT
    Q_ENUMS(Status)
public:
    explicit VerifyChecksumsDialog(QWidget *parent = nullptr);
    ~VerifyChecksumsDialog();

    enum Status {
        Unknown,
        OK,
        Failed,
        Error,
        NumStatii
    };

public Q_SLOTS:
    void setBaseDirectories(const QStringList &bases);
    void setProgress(int current, int total);
    void setStatus(const QString &file, Kleo::Crypto::Gui::VerifyChecksumsDialog::Status status);
    void setErrors(const QStringList &errors);
    void clearStatusInformation();

Q_SIGNALS:
    void canceled();

private:
    Q_PRIVATE_SLOT(d, void slotErrorButtonClicked())
    class Private;
    kdtools::pimpl_ptr<Private> d;
};
}
}
}

Q_DECLARE_METATYPE(Kleo::Crypto::Gui::VerifyChecksumsDialog::Status)

#endif // QT_NO_DIRMODEL

#endif // __KLEOPATRA_CRYPTO_GUI_RESULTITEMWIDGET_H__
