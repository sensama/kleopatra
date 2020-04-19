/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/signencryptfileswizard.h

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

#ifndef __KLEOPATRA_CRYPTO_GUI_SIGNENCRYPTFILESWIZARD_H__
#define __KLEOPATRA_CRYPTO_GUI_SIGNENCRYPTFILESWIZARD_H__

#include <utils/pimpl_ptr.h>

#include <gpgme++/global.h>

#include <QWizard>

#include <QVector>
#include <QMap>

#include <memory>


namespace GpgME
{
class Key;
}

namespace Kleo
{
namespace Crypto
{
class TaskCollection;
}
}

class ResultPage;
class SigEncPage;

namespace Kleo
{

class SignEncryptFilesWizard : public QWizard
{
    Q_OBJECT
public:
    enum KindNames{
        SignatureCMS,
        CombinedPGP,
        EncryptedPGP,
        EncryptedCMS,
        SignaturePGP,
        Directory
    };

    explicit SignEncryptFilesWizard(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~SignEncryptFilesWizard();

    // Inputs
    void setSigningPreset(bool preset);
    void setSigningUserMutable(bool mut);

    void setEncryptionPreset(bool preset);
    void setEncryptionUserMutable(bool mut);

    void setArchiveForced(bool archive);
    void setArchiveMutable(bool archive);

    void setOutputNames(const QMap<int, QString> &nameMap) const;
    QMap<int, QString> outputNames() const;

    void setTaskCollection(const std::shared_ptr<Kleo::Crypto::TaskCollection> &coll);

    // Outputs
    QVector<GpgME::Key> resolvedRecipients() const;
    QVector<GpgME::Key> resolvedSigners() const;
    bool encryptSymmetric() const;

    void setLabelText(const QString &label) const;

protected:
    void readConfig();
    void writeConfig();

Q_SIGNALS:
    void operationPrepared();

private Q_SLOTS:
    void slotCurrentIdChanged(int);

private:
    SigEncPage *mSigEncPage;
    ResultPage *mResultPage;
    QAbstractButton *mLabel;
    bool mSigningUserMutable,
         mEncryptionUserMutable;
};

}

#endif /* __KLEOPATRA_CRYPTO_GUI_SIGNENCRYPTFILESWIZARD_H__ */
