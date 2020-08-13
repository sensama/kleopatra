/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/signingcertificateselectionwidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007, 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef __KLEOPATRA_CRYPTO_GUI_SIGNINGCERTIFICATESELECTIONWIDGET_H__
#define __KLEOPATRA_CRYPTO_GUI_SIGNINGCERTIFICATESELECTIONWIDGET_H__

#include <QWidget>

#include <gpgme++/global.h>

#include <utils/pimpl_ptr.h>

template <typename K, typename U> class QMap;

namespace GpgME
{
class Key;
}

namespace Kleo
{
namespace Crypto
{
namespace Gui
{

class SigningCertificateSelectionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SigningCertificateSelectionWidget(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~SigningCertificateSelectionWidget();

    void setAllowedProtocols(const QVector<GpgME::Protocol> &allowedProtocols);
    void setAllowedProtocols(bool pgp, bool cms);
    void setSelectedCertificates(const QMap<GpgME::Protocol, GpgME::Key> &certificates);
    void setSelectedCertificates(const GpgME::Key &pgp, const GpgME::Key &cms);
    QMap<GpgME::Protocol, GpgME::Key> selectedCertificates() const;

    bool rememberAsDefault() const;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

}
}
}

#endif // __KLEOPATRA_CRYPTO_GUI_SIGNINGCERTIFICATESELECTIONWIDGET_H__

