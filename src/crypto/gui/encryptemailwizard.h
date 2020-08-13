/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/encryptemailwizard.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_ENCRYPTEMAILWIZARD_H__
#define __KLEOPATRA_ENCRYPTEMAILWIZARD_H__

#include <crypto/gui/signencryptwizard.h>

namespace Kleo
{
namespace Crypto
{
namespace Gui
{

class EncryptEMailWizard : public SignEncryptWizard
{
    Q_OBJECT
public:
    explicit EncryptEMailWizard(QWidget *parent = nullptr, Qt::WindowFlags flags = {});
    ~EncryptEMailWizard();

    bool quickMode() const;
    void setQuickMode(bool quick);

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};
}
}
}
#endif // __KLEOPATRA_ENCRYPTEMAILWIZARD_H__
