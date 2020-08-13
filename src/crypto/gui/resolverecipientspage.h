/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/resolverecipientspage.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_CRYPTO_GUI_RESOLVERECIPIENTSPAGE_H__
#define __KLEOPATRA_CRYPTO_GUI_RESOLVERECIPIENTSPAGE_H__

#include <crypto/gui/wizardpage.h>

#include <utils/pimpl_ptr.h>

#include <gpgme++/global.h>

#include <memory>
#include <vector>

namespace GpgME
{
class Key;
}

namespace KMime
{
namespace Types
{
class Mailbox;
}
}

namespace Kleo
{
namespace Crypto
{

class RecipientPreferences;

namespace Gui
{

class ResolveRecipientsPage : public WizardPage
{
    Q_OBJECT
public:
    explicit ResolveRecipientsPage(QWidget *parent = nullptr);
    ~ResolveRecipientsPage() override;

    bool isComplete() const override;

    /**
     * The protocol selected by the user (which is chosen by
     * the user in case none was preset)
     */
    GpgME::Protocol selectedProtocol() const;

    /**
     * the protocol set before the dialog is shown. Defaults to
     * GpgME::UnknownProtocol */
    GpgME::Protocol presetProtocol() const;
    void setPresetProtocol(GpgME::Protocol protocol);

    bool multipleProtocolsAllowed() const;
    void setMultipleProtocolsAllowed(bool allowed);

    bool symmetricEncryptionSelected() const;
    void setSymmetricEncryptionSelected(bool enabled);

    bool symmetricEncryptionSelectable() const;
    void setSymmetricEncryptionSelectable(bool selectable);

    /** if true, the user is allowed to remove/add recipients via the UI.
     * Defaults to @p false.
     */
    bool recipientsUserMutable() const;
    void setRecipientsUserMutable(bool isMutable);

    void setAdditionalRecipientsInfo(const std::vector<GpgME::Key> &recipients);

    void setRecipients(const std::vector<KMime::Types::Mailbox> &recipients, const std::vector<KMime::Types::Mailbox> &encryptToSelfRecipients);
    std::vector<GpgME::Key> resolvedCertificates() const;

    std::shared_ptr<RecipientPreferences> recipientPreferences() const;
    void setRecipientPreferences(const std::shared_ptr<RecipientPreferences> &prefs);

Q_SIGNALS:
    void selectedProtocolChanged();

private:
    void onNext() override;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void selectionChanged())
    Q_PRIVATE_SLOT(d, void protocolSelected(int))
    Q_PRIVATE_SLOT(d, void addRecipient())
    Q_PRIVATE_SLOT(d, void removeSelectedEntries())
    Q_PRIVATE_SLOT(d, void completeChangedInternal())
    class ListWidget;
    class ItemWidget;
};

}
}
}

#endif // __KLEOPATRA_CRYPTO_GUI_RESOLVERECIPIENTSPAGE_H__
