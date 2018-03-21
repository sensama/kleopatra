/* -*- mode: c++; c-basic-offset:4 -*-
    importcertificatefromdatacommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

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

#include <config-kleopatra.h>

#include "importcertificatefromdatacommand.h"
#include "importcertificatescommand_p.h"

#include <KLocalizedString>

#include <memory>

using namespace GpgME;
using namespace Kleo;
using namespace QGpgME;

class ImportCertificateFromDataCommand::Private : public ImportCertificatesCommand::Private
{
    friend class ::ImportCertificateFromDataCommand;
    ImportCertificateFromDataCommand *q_func() const
    {
        return static_cast<ImportCertificateFromDataCommand *>(q);
    }
public:
    explicit Private(ImportCertificateFromDataCommand *qq,
                     const QByteArray &data, GpgME::Protocol proto);
    ~Private();

private:
    QByteArray mData;
    GpgME::Protocol mProto;
};

ImportCertificateFromDataCommand::Private *ImportCertificateFromDataCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const ImportCertificateFromDataCommand::Private *ImportCertificateFromDataCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

ImportCertificateFromDataCommand::Private::Private(ImportCertificateFromDataCommand *qq,
                                                   const QByteArray &data,
                                                   GpgME::Protocol proto)
    : ImportCertificatesCommand::Private(qq, nullptr), mData(data), mProto(proto)
{
}

ImportCertificateFromDataCommand::Private::~Private() {}

#define d d_func()
#define q q_func()

ImportCertificateFromDataCommand::ImportCertificateFromDataCommand(const QByteArray &data,
                                                                   GpgME::Protocol proto)
    : ImportCertificatesCommand(new Private(this, data, proto))
{
}

ImportCertificateFromDataCommand::~ImportCertificateFromDataCommand() {}

void ImportCertificateFromDataCommand::doStart()
{
    d->startImport(d->mProto, d->mData, i18n("Notepad"));
}

#undef d
#undef q
