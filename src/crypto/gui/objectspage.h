/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/objectspage.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_CRYPTO_GUI_OBJECTSPAGE_H__
#define __KLEOPATRA_CRYPTO_GUI_OBJECTSPAGE_H__

#include <crypto/gui/wizardpage.h>

#include <utils/pimpl_ptr.h>

class QStringList;

namespace Kleo
{
namespace Crypto
{
namespace Gui
{

class ObjectsPage : public WizardPage
{
    Q_OBJECT
public:
    explicit ObjectsPage(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~ObjectsPage() override;

    bool isComplete() const override;
    void setFiles(const QStringList &files);
    QStringList files() const;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void add())
    Q_PRIVATE_SLOT(d, void remove())
    Q_PRIVATE_SLOT(d, void listSelectionChanged())
};

}
}
}

#endif // __KLEOPATRA_CRYPTO_GUI_OBJECTSPAGE_H__

