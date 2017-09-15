/* -*- mode: c++; c-basic-offset:4 -*-
    autodecryptverifyfilescontroller.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB
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

#ifndef __KLEOPATRA_CRYPTO_AUTODECRYPTVERIFYFILESCONTROLLER_H__
#define __KLEOPATRA_CRYPTO_AUTODECRYPTVERIFYFILESCONTROLLER_H__

#include "crypto/decryptverifyfilescontroller.h"

#include <utils/types.h>


#include <memory>
#include <vector>

namespace Kleo
{
namespace Crypto
{

class AutoDecryptVerifyFilesController : public DecryptVerifyFilesController
{
    Q_OBJECT
public:
    explicit AutoDecryptVerifyFilesController(QObject *parent = nullptr);
    explicit AutoDecryptVerifyFilesController(const std::shared_ptr<const ExecutionContext> &ctx, QObject *parent = nullptr);

    ~AutoDecryptVerifyFilesController();

    void setFiles(const QStringList &files) override;
    void setOperation(DecryptVerifyOperation op) override;
    DecryptVerifyOperation operation() const override;
    void start() override;

public Q_SLOTS:
    void cancel() override;

private:
    void doTaskDone(const Task *task, const std::shared_ptr<const Task::Result> &) override;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotDialogCanceled())
    Q_PRIVATE_SLOT(d, void schedule())
};

}
}

#endif // __KLEOPATRA_CRYPTO_AUTODECRYPTVERIFYFILESCONTROLLER_H__
