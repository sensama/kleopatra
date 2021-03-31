/*  crypto/gui/certificatelineedit.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <Libkleo/KeyGroup>

#include <QLineEdit>

#include <gpgme++/key.h>

#include <memory>

class QCompleter;
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

    KeyGroup group() const;

    /** Check if the text is empty */
    bool isEmpty() const;

    /** Set the preselected Key for this widget. */
    void setKey(const GpgME::Key &key);

    /** Set the preselected group for this widget. */
    void setGroup(const KeyGroup &group);

    /** Set the used keyfilter. */
    void setKeyFilter(const std::shared_ptr<KeyFilter> &filter);

Q_SIGNALS:
    /** Emitted when the selected key changed. */
    void keyChanged();

    /** Emitted when the entry is empty and editing is finished. */
    void wantsRemoval(CertificateLineEdit *w);

    /** Emitted when the entry is no longer empty. */
    void editingStarted();

    /** Emitted when the details dialog or the selection dialog is requested. */
    void dialogRequested();

private Q_SLOTS:
    void updateKey();
    void editChanged();
    void editFinished();
    void checkLocate();

private:
    KeyListSortFilterProxyModel *const mFilterModel;
    KeyListSortFilterProxyModel *const mCompleterFilterModel;
    QCompleter *mCompleter = nullptr;
    QLabel *mStatusLabel,
           *mStatusIcon;
    GpgME::Key mKey;
    KeyGroup mGroup;
    GpgME::Protocol mCurrentProto;
    std::shared_ptr<KeyFilter> mFilter;
    bool mEditStarted = false;
    bool mEditFinished = false;
    QAction *const mLineAction;
};
}
