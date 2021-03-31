/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/signemailwizard.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <crypto/gui/signencryptwizard.h>

#include <utils/pimpl_ptr.h>

namespace Kleo
{
namespace Crypto
{
namespace Gui
{

class SignEMailWizard : public SignEncryptWizard
{
    Q_OBJECT
public:
    explicit SignEMailWizard(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~SignEMailWizard();

    bool quickMode() const;
    void setQuickMode(bool quick);

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

}
}
}

