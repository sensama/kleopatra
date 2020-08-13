/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/createchecksumscontroller.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_CRYPTO_CREATECHECKSUMSCONTROLLER_H__
#define __KLEOPATRA_CRYPTO_CREATECHECKSUMSCONTROLLER_H__

#include <crypto/controller.h>

#include <utils/pimpl_ptr.h>

#include <gpgme++/global.h>
#include <kmime/kmime_header_parsing.h>

#include <memory>
#include <vector>

namespace Kleo
{
namespace Crypto
{

class CreateChecksumsController : public Controller
{
    Q_OBJECT
public:
    explicit CreateChecksumsController(QObject *parent = nullptr);
    explicit CreateChecksumsController(const std::shared_ptr<const ExecutionContext> &ctx, QObject *parent = nullptr);
    ~CreateChecksumsController();

    void setAllowAddition(bool allow);
    bool allowAddition() const;

    void setFiles(const QStringList &files);

    void start();

public Q_SLOTS:
    void cancel();

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotOperationFinished())
    Q_PRIVATE_SLOT(d, void slotProgress(int, int, QString))
};

}
}

#endif /* __KLEOPATRA_UISERVER_CREATECHECKSUMSCONTROLLER_H__ */

