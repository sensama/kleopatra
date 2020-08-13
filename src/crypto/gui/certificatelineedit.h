/*  crypto/gui/certificatelineedit.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
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
