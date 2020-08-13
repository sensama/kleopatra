/* -*- mode: c++; c-basic-offset:4 -*-
    uiserver/verifychecksumscommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UISERVER_VERIFYCHECKSUMSCOMMAND_H__
#define __KLEOPATRA_UISERVER_VERIFYCHECKSUMSCOMMAND_H__

#include "assuancommand.h"

#ifndef QT_NO_DIRMODEL

#include <QObject>

namespace Kleo
{

class VerifyChecksumsCommand : public QObject, public AssuanCommandMixin<VerifyChecksumsCommand>
{
    Q_OBJECT
public:
    VerifyChecksumsCommand();
    ~VerifyChecksumsCommand();

    static const char *staticName()
    {
        return "CHECKSUM_VERIFY_FILES";
    }

private:
    int doStart() override;
    void doCanceled() override;

#ifdef Q_MOC_RUN
private Q_SLOTS:
    void done();
    void done(int, QString);
#endif

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    //Q_PRIVATE_SLOT( this, void done() )
    //Q_PRIVATE_SLOT( this, void done(int,QString) )
};

}

#endif // QT_NO_DIRMODEL

#endif /* __KLEOPATRA_UISERVER_VERIFYCHECKSUMSCOMMAND_H__ */
