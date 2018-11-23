/*  crypto/gui/certificatelineedit.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2016 by Bundesamt für Sicherheit in der Informationstechnik
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
#ifndef CRYPTO_GUI_CERTIFICATELINEEDIT_H
#define CRYPTO_GUI_CERTIFICATELINEEDIT_H

#include <QLineEdit>

#include <gpgme++/key.h>

#include "dialogs/certificateselectiondialog.h"

#include <memory>

class QLabel;
class QAction;

namespace Kleo
{
class AbstractKeyListModel;
class KeyFilter;
class KeyListSortFilterProxyModel;

/** Line edit and completion based Certificate Selection Widget.
 *
 * Shows the status of the selection with a status label and icon.
 *
 * The widget will use a single line HBox Layout. For larger dialog
 * see certificateslectiondialog.
 */
class CertificateLineEdit: public QLineEdit
{
    Q_OBJECT
public:
    /** Create the certificate selection line.
     *
     * If parent is not NULL the model is not taken
     * over but the parent argument used as the parent of the model.
     *
     * @param model: The keylistmodel to use.
     * @param parent: The usual widget parent.
     * @param filter: The filters to use. See certificateselectiondialog.
     */
    CertificateLineEdit(AbstractKeyListModel *model,
                        QWidget *parent = nullptr,
                        KeyFilter *filter = nullptr);

    /** Get the selected key */
    GpgME::Key key() const;

    /** Check if the text is empty */
    bool isEmpty() const;

    /** Set the preselected Key for this widget. */
    void setKey(const GpgME::Key &key);

    /** Set the used keyfilter. */
    void setKeyFilter(const std::shared_ptr<KeyFilter> &filter);

Q_SIGNALS:
    /** Emitted when the selected key changed. */
    void keyChanged();

    /** Emitted when the entry is empty and editing is finished. */
    void wantsRemoval(CertificateLineEdit *w);

    /** Emitted when the entry is no longer empty. */
    void editingStarted();

    /** Emitted when the certselectiondialog resulted in multiple certificates. */
    void addRequested(const GpgME::Key &key);

private Q_SLOTS:
    void updateKey();
    void dialogRequested();
    void editChanged();
    void checkLocate();

private:
    KeyListSortFilterProxyModel *mFilterModel;
    QLabel *mStatusLabel,
           *mStatusIcon;
    GpgME::Key mKey;
    GpgME::Protocol mCurrentProto;
    std::shared_ptr<KeyFilter> mFilter;
    bool mEditStarted,
         mEditFinished;
    QAction *mLineAction;
};
}
#endif
