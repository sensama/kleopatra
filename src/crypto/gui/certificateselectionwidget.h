/*  crypto/gui/certificateselectionwidget.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2016 Intevation GmbH

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
#ifndef CRYPTO_GUI_CERTIFICATESELECTIONWIDGET_H
#define CRYPTO_GUI_CERTIFICATESELECTIONWIDGET_H

class QLineEdit;

#include <QWidget>
#include <QString>

#include "dialogs/certificateselectiondialog.h"

namespace Kleo
{
class AbstractKeyListModel;
class KeyListSortFilterProxyModel;
class CertificateComboBox;

/** Generic Certificate Selection Widget.
 *
 * This class does not care about protocols. By default it will
 * prefer OpenPGP. Uses the KeyCache directly to fill the choices for
 * the selected capabilities.
 *
 * The widget will use a single line HBox Layout. For larger dialog
 * see certificateslectiondialog.
 */
class CertificateSelectionWidget: public QWidget
{
    Q_OBJECT
public:
    /** Create the certificate selection line.
     *
     * @param parent: The usual widget parent.
     * @param options: The options to use. See certificateselectiondialog.
     * @param mailbox: If a mailbox entry should be shown.
     * @param defaultFpr: The default fingerprint to fill this with.
     */
    CertificateSelectionWidget(QWidget *parent = Q_NULLPTR,
                               int otions = Dialogs::CertificateSelectionDialog::AnyFormat,
                               bool mailbox = true,
                               const QString &defaultFpr = QString());

    /** Get the selected key */
    GpgME::Key key() const;

Q_SIGNALS:
    /** Emited when the selected key changed. */
    void keyChanged();

private Q_SLOTS:
    void keysMayHaveChanged();
    void mailEntryChanged();

private:
    CertificateComboBox *mCombo;
    QLineEdit *mMailEntry;
    int mOptions;
    AbstractKeyListModel *mModel;
    KeyListSortFilterProxyModel *mFilterModel;
};
}
#endif // CRYPTO_GUI_CERTIFICATESELECTIONWIDGET_H
