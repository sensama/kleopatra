/* -*- mode: c++; c-basic-offset:4 -*-
    importcertificatefromdatacommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
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
