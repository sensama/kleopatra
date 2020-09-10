/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/signingcertificateselectiondialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef __KLEOPATRA_CRYPTO_GUI_SIGNINGCERTIFICATESELECTIONDIALOG_H__
#define __KLEOPATRA_CRYPTO_GUI_SIGNINGCERTIFICATESELECTIONDIALOG_H__

#include <QDialog>

#include <gpgme++/key.h>

#include <utils/pimpl_ptr.h>

template <typename K, typename U> class QMap;

namespace Kleo
{
namespace Crypto
{
namespace Gui
{

class SigningCertificateSelectionWidget;

class SigningCertificateSelectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SigningCertificateSelectionDialog(QWidget *parent = nullptr);
    ~SigningCertificateSelectionDialog();

    void setAllowedProtocols(const QVector<GpgME::Protocol> &allowedProtocols);
    void setSelectedCertificates(const QMap<GpgME::Protocol, GpgME::Key> &certificates);
    Q_REQUIRED_RESULT QMap<GpgME::Protocol, GpgME::Key> selectedCertificates() const;

    Q_REQUIRED_RESULT bool rememberAsDefault() const;

private:
    SigningCertificateSelectionWidget *const widget;
};

}
}
}

#endif // __KLEOPATRA_CRYPTO_GUI_SIGNINGCERTIFICATESELECTIONDIALOG_H__

