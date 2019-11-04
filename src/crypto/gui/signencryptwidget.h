/*  crypto/gui/signencryptwidget.h

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
#ifndef CRYPTO_GUI_SIGNENCRYPTWIDGET_H
#define CRYPTO_GUI_SIGNENCRYPTWIDGET_H

#include <QWidget>
#include <QVector>
#include <gpgme++/key.h>

class QGridLayout;
class QCheckBox;

namespace Kleo
{
class CertificateLineEdit;
class KeySelectionCombo;
class AbstractKeyListModel;
class UnknownRecipientWidget;

class SignEncryptWidget: public QWidget
{
    Q_OBJECT
public:
    /** If cmsSigEncExclusive is true CMS operations can be
     * done only either as sign or as encrypt */
    explicit SignEncryptWidget(QWidget *parent = nullptr, bool cmsSigEncExclusive = false);

    /** Returns the list of recipients selected in the dialog
     * or an empty list if encryption is disabled */
    QVector <GpgME::Key> recipients() const;

    /** Returns the selected signing key or a null key if signing
     * is disabled. */
    GpgME::Key signKey() const;

    /** Returns the selected encrypt to self key or a null key if
     * encrypt to self is disabled. */
    GpgME::Key selfKey() const;

    /** Returns the operation based on the current selection or
     * a null string if nothing would happen. */
    QString currentOp() const;

    /** Whether or not symmetric encryption should also be used. */
    bool encryptSymmetric() const;

    /** Save the currently selected signing and encrypt to self keys. */
    void saveOwnKeys() const;

    /** Return whether or not all keys involved in the operation are
        compliant with CO_DE_VS, and all keys are valid (i.e. all
        userIDs have Validity >= Full).  */
    bool isDeVsAndValid() const;

    /** Set whether or not signing group should be checked */
    void setSigningChecked(bool value);

    /** Set whether or not encryption group should be checked */
    void setEncryptionChecked(bool value);

    /** Filter for a specific protocol. Use UnknownProtocol for both
     * S/MIME and OpenPGP */
    void setProtocol(GpgME::Protocol protocol);

    /** Add a recipient with the key key */
    void addRecipient(const GpgME::Key &key);

    /** Add a placehoder for an unknown key */
    void addUnknownRecipient(const char *keyId);

    /** Remove all Recipients added by keyId or by key. */
    void clearAddedRecipients();

    /** Remove a Recipient key */
    void removeRecipient(const GpgME::Key &key);

    /** Validate that each line edit with content has a key. */
    bool validate();

protected Q_SLOTS:
    void updateOp();
    void recipientsChanged();
    void recpRemovalRequested(CertificateLineEdit *w);
    void addRecipient();

protected:
    void loadKeys();

Q_SIGNALS:
    /* Emitted when the certificate selection changed the operation
     * with that selection. e.g. "Sign" or "Sign/Encrypt".
     * If no crypto operation is selected this returns a null string. */
    void operationChanged(const QString &op);

    /* Emitted when the certificate selection might be changed. */
    void keysChanged();

private:
    KeySelectionCombo *mSigSelect,
                      *mSelfSelect;
    QVector<CertificateLineEdit *> mRecpWidgets;
    QVector<UnknownRecipientWidget *> mUnknownWidgets;
    QVector<GpgME::Key> mAddedKeys;
    QGridLayout *mRecpLayout;
    QString mOp;
    AbstractKeyListModel *mModel;
    QCheckBox *mSymmetric,
              *mSigChk,
              *mEncOtherChk,
              *mEncSelfChk;
    int mRecpRowCount;
    GpgME::Protocol mCurrentProto;
    bool mIsExclusive;
};
} // namespace Kleo
#endif // CRYPTO_GUI_SIGNENCRYPTWIDGET_H
