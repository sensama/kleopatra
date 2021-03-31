/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/certificateresolver.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <utils/pimpl_ptr.h>

#include <gpgme++/key.h>
#include <kmime/kmime_header_parsing.h>

#include <KSharedConfig>

#include <vector>

class KConfig;

namespace GpgME
{
class Key;
}

namespace Kleo
{
namespace Crypto
{

class SigningPreferences
{
public:
    virtual ~SigningPreferences() {}
    virtual GpgME::Key preferredCertificate(GpgME::Protocol protocol) = 0;
    virtual void setPreferredCertificate(GpgME::Protocol protocol, const GpgME::Key &certificate) = 0;

};

class RecipientPreferences
{
public:
    virtual ~RecipientPreferences() {}
    virtual GpgME::Key preferredCertificate(const KMime::Types::Mailbox &recipient, GpgME::Protocol protocol) = 0;
    virtual void setPreferredCertificate(const KMime::Types::Mailbox &recipient, GpgME::Protocol protocol, const GpgME::Key &certificate) = 0;
};

class KConfigBasedRecipientPreferences : public RecipientPreferences
{
public:
    explicit KConfigBasedRecipientPreferences(const KSharedConfigPtr &config);
    ~KConfigBasedRecipientPreferences() override;
    GpgME::Key preferredCertificate(const KMime::Types::Mailbox &recipient, GpgME::Protocol protocol) override;
    void setPreferredCertificate(const KMime::Types::Mailbox &recipient, GpgME::Protocol protocol, const GpgME::Key &certificate) override;
private:
    Q_DISABLE_COPY(KConfigBasedRecipientPreferences)
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

class KConfigBasedSigningPreferences : public SigningPreferences
{
public:
    explicit KConfigBasedSigningPreferences(const KSharedConfigPtr &config);
    ~KConfigBasedSigningPreferences() override;
    GpgME::Key preferredCertificate(GpgME::Protocol protocol) override;
    void setPreferredCertificate(GpgME::Protocol protocol, const GpgME::Key &certificate) override;
private:
    Q_DISABLE_COPY(KConfigBasedSigningPreferences)
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

class CertificateResolver
{
public:
    static std::vector< std::vector<GpgME::Key> > resolveRecipients(const std::vector<KMime::Types::Mailbox> &recipients, GpgME::Protocol proto);
    static std::vector<GpgME::Key> resolveRecipient(const KMime::Types::Mailbox &recipient, GpgME::Protocol proto);

    static std::vector< std::vector<GpgME::Key> > resolveSigners(const std::vector<KMime::Types::Mailbox> &signers, GpgME::Protocol proto);
    static std::vector<GpgME::Key> resolveSigner(const KMime::Types::Mailbox &signer, GpgME::Protocol proto);
};

}
}

