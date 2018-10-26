/*  crypto/gui/certificateselectionline.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2009 Klarälvdalens Datakonsult AB
                  2016 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#ifndef CRYPTO_GUI_CERTIFICATESELECTIONLINE_H
#define CRYPTO_GUI_CERTIFICATESELECTIONLINE_H

#include <QString>
#include <vector>
#include <QGridLayout>

#include <gpgme++/key.h>

class QToolButton;
class QLabel;
class QStackedWidget;

namespace Kleo
{

class KeysComboBox;

class CertificateSelectionLine
{
public:
    /** Number of columns needed in the gridlayout. */
    static const unsigned int NumColumns = 4;

    /** Create a certificateselection line that distinguishes between protocols.
     *
     * Ambiguity means here that there is not a single valid choice or that
     * soemthing is not selected. There are basically two modes here, one is
     * for a protocol specific selection, that is used mainly by GpgOL over assuan.
     * This is the constructor for that mode. The second mode does not care about
     * protocols.
     *
     * @param toFrom: An optional label.
     * @param mailbox: The Mailbox for which the certificate should be selected.
     * @param pgp: List of PGP Certificates.
     * @param pgpAmbig: Whether or not the PGP Cert is ambiguous.
     * @param cms: List of CMS Certificates.
     * @param cmsAmbig: Whether or not the CMS Cert is ambiguous.
     * @param q: Parent widget.
     * @param glay: Layout to add the widgets to.
     */
    CertificateSelectionLine(const QString &toFrom, const QString &mailbox,
                             const std::vector<GpgME::Key> &pgp, bool pgpAmbig,
                             const std::vector<GpgME::Key> &cms, bool cmsAmbig,
                             QWidget *q, QGridLayout &glay);

    void showHide(GpgME::Protocol proto, bool &first, bool showAll, bool op) const;

    bool wasInitiallyAmbiguous(GpgME::Protocol proto) const;

    bool isStillAmbiguous(GpgME::Protocol proto) const;

    QString mailboxText() const;

    void addAndSelectCertificate(const GpgME::Key &key) const;

    GpgME::Key key(GpgME::Protocol proto) const;

    const QToolButton *toolButton() const;

    void kill();

    KeysComboBox *comboBox(GpgME::Protocol proto) const;

private:
    bool pgpAmbiguous : 1;
    bool cmsAmbiguous : 1;

    QLabel *mToFromLB;
    QLabel *mMailboxLB;
    QStackedWidget *mSbox;
    KeysComboBox *mPgpCB,
                 *mCmsCB,
                 *noProtocolCB;
    QToolButton *mToolTB;
};

} // namespace Kleo

#endif // CRYPTO_GUI_CERTIFICATESELECTIONLINE_H
