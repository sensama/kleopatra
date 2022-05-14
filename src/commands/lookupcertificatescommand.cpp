/* -*- mode: c++; c-basic-offset:4 -*-
    commands/lookupcertificatescommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008, 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "lookupcertificatescommand.h"

#include "importcertificatescommand_p.h"

#include "detailscommand.h"

#include <settings.h>

#include "view/tabwidget.h"

#include <Libkleo/Compat>
#include <Libkleo/GnuPG>

#include <dialogs/lookupcertificatesdialog.h>

#include <Libkleo/Algorithm>
#include <Libkleo/Formatting>
#include <Libkleo/Stl_Util>

#include <QGpgME/CryptoConfig>
#include <QGpgME/Protocol>
#include <QGpgME/KeyListJob>
#include <QGpgME/ImportFromKeyserverJob>
#ifdef QGPGME_SUPPORTS_WKDLOOKUP
# include <QGpgME/WKDLookupJob>
# include <QGpgME/WKDLookupResult>
#endif

#include <gpgme++/data.h>
#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>
#include <gpgme++/importresult.h>

#include <KLocalizedString>
#include <KMessageBox>
#include "kleopatra_debug.h"

#include <QRegularExpression>

#include <vector>
#include <map>
#include <set>
#include <algorithm>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;
using namespace GpgME;
using namespace QGpgME;

static QDebug operator<<(QDebug s, const GpgME::Key &key)
{
    if (key.primaryFingerprint()) {
        return s << Formatting::summaryLine(key) << "fpr:" << key.primaryFingerprint();
    } else {
        return s << Formatting::summaryLine(key) << "id:" << key.keyID();
    }
}
class LookupCertificatesCommand::Private : public ImportCertificatesCommand::Private
{
    friend class ::Kleo::Commands::LookupCertificatesCommand;
    LookupCertificatesCommand *q_func() const
    {
        return static_cast<LookupCertificatesCommand *>(q);
    }
public:
    explicit Private(LookupCertificatesCommand *qq, KeyListController *c);
    ~Private() override;

    void init();

private:
    void slotSearchTextChanged(const QString &str);
    void slotNextKey(const Key &key);
    void slotKeyListResult(const KeyListResult &result);
#ifdef QGPGME_SUPPORTS_WKDLOOKUP
    void slotWKDLookupResult(const WKDLookupResult &result);
#endif
    void tryToFinishKeyLookup();
    void slotImportRequested(const std::vector<Key> &keys);
    void slotDetailsRequested(const Key &key);
    void slotSaveAsRequested(const std::vector<Key> &keys);
    void slotDialogRejected()
    {
        canceled();
    }

private:
    using ImportCertificatesCommand::Private::showError;
    void showError(QWidget *parent, const KeyListResult &result);
    void showResult(QWidget *parent, const KeyListResult &result);
    void createDialog();
    KeyListJob *createKeyListJob(GpgME::Protocol proto) const
    {
        const auto cbp = (proto == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
        return cbp ? cbp->keyListJob(true) : nullptr;
    }
#ifdef QGPGME_SUPPORTS_WKDLOOKUP
    WKDLookupJob *createWKDLookupJob() const
    {
        const auto cbp = QGpgME::openpgp();
        return cbp ? cbp->wkdLookupJob() : nullptr;
    }
#endif
    ImportFromKeyserverJob *createImportJob(GpgME::Protocol proto) const
    {
        const auto cbp = (proto == GpgME::OpenPGP) ? QGpgME::openpgp() : QGpgME::smime();
        return cbp ? cbp->importFromKeyserverJob() : nullptr;
    }
    void startKeyListJob(GpgME::Protocol proto, const QString &str);
#ifdef QGPGME_SUPPORTS_WKDLOOKUP
    void startWKDLookupJob(const QString &str);
#endif
    bool checkConfig() const;

    QWidget *dialogOrParentWidgetOrView() const
    {
        if (dialog) {
            return dialog;
        } else {
            return parentWidgetOrView();
        }
    }

private:
    GpgME::Protocol protocol = GpgME::UnknownProtocol;
    QString query;
    bool autoStartLookup = false;
    QPointer<LookupCertificatesDialog> dialog;
    struct KeyListingVariables {
        QPointer<KeyListJob> cms, openpgp;
#ifdef QGPGME_SUPPORTS_WKDLOOKUP
        QPointer<WKDLookupJob> wkdJob;
#endif
        QString pattern;
        KeyListResult result;
        std::vector<Key> keys;
        int numKeysWithoutUserId = 0;
        std::set<std::string> wkdKeyFingerprints;
        QByteArray wkdKeyData;
        QString wkdSource;
        bool cmsKeysHaveNoFingerprints = false;
        bool openPgpKeysHaveNoFingerprints = false;

        void reset()
        {
            *this = KeyListingVariables();
        }
    } keyListing;
};

LookupCertificatesCommand::Private *LookupCertificatesCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const LookupCertificatesCommand::Private *LookupCertificatesCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

LookupCertificatesCommand::Private::Private(LookupCertificatesCommand *qq, KeyListController *c)
    : ImportCertificatesCommand::Private(qq, c),
      dialog()
{
    if (!Settings{}.cmsEnabled()) {
        protocol = GpgME::OpenPGP;
    }
}

LookupCertificatesCommand::Private::~Private()
{
    qCDebug(KLEOPATRA_LOG);
    delete dialog;
}

LookupCertificatesCommand::LookupCertificatesCommand(KeyListController *c)
    : ImportCertificatesCommand(new Private(this, c))
{
    d->init();
}

LookupCertificatesCommand::LookupCertificatesCommand(const QString &query, KeyListController *c)
    : ImportCertificatesCommand(new Private(this, c))
{
    d->init();
    d->query = query;
    d->autoStartLookup = true;
}

LookupCertificatesCommand::LookupCertificatesCommand(QAbstractItemView *v, KeyListController *c)
    : ImportCertificatesCommand(v, new Private(this, c))
{
    d->init();
    if (c->tabWidget()) {
        d->query = c->tabWidget()->stringFilter();
        // do not start the lookup automatically to prevent unwanted leaking
        // of information
    }
}

void LookupCertificatesCommand::Private::init()
{

}

LookupCertificatesCommand::~LookupCertificatesCommand()
{
    qCDebug(KLEOPATRA_LOG);
}

void LookupCertificatesCommand::setProtocol(GpgME::Protocol protocol)
{
    d->protocol = protocol;
}

GpgME::Protocol LookupCertificatesCommand::protocol() const
{
    return d->protocol;
}

void LookupCertificatesCommand::doStart()
{

    if (!d->checkConfig()) {
        d->finished();
        return;
    }

    d->createDialog();
    Q_ASSERT(d->dialog);

    // if we have a prespecified query, load it into find field
    // and start the search, if auto-start is enabled
    if (!d->query.isEmpty()) {
        d->dialog->setSearchText(d->query);
        if (d->autoStartLookup) {
            d->slotSearchTextChanged(d->query);
        }
    } else {
        d->dialog->setPassive(false);
    }

    d->dialog->show();

}

void LookupCertificatesCommand::Private::createDialog()
{
    if (dialog) {
        return;
    }
    dialog = new LookupCertificatesDialog;
    applyWindowID(dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &LookupCertificatesDialog::searchTextChanged, q, [this](const QString &text) { slotSearchTextChanged(text); });
    using CertsVec = std::vector<GpgME::Key>;
    connect(dialog, &LookupCertificatesDialog::saveAsRequested, q, [this](const CertsVec &certs) { slotSaveAsRequested(certs); });
    connect(dialog, &LookupCertificatesDialog::importRequested, q, [this](const CertsVec &certs) { slotImportRequested(certs); });
    connect(dialog, &LookupCertificatesDialog::detailsRequested, q, [this](const GpgME::Key &gpgKey) { slotDetailsRequested(gpgKey); });
    connect(dialog, &QDialog::rejected, q, [this]() { slotDialogRejected(); });
}

static auto searchTextToEmailAddress(const QString &s)
{
    return QString::fromStdString(UserID::addrSpecFromString(s.toStdString().c_str()));
}

void LookupCertificatesCommand::Private::slotSearchTextChanged(const QString &str)
{
    // pressing return might trigger both search and dialog destruction (search focused and default key set)
    // On Windows, the dialog is then destroyed before this slot is called
    if (dialog) {   //thus test
        dialog->setPassive(true);
        dialog->setCertificates(std::vector<Key>());
        dialog->showInformation({});
    }

    query = str;

    keyListing.reset();
    keyListing.pattern = str;

    if (protocol != GpgME::OpenPGP) {
        startKeyListJob(CMS, str);
    }

    if (protocol != GpgME::CMS) {
        static const QRegularExpression rx(QRegularExpression::anchoredPattern(QLatin1String("(?:0x|0X)?[0-9a-fA-F]{6,}")));
        if (rx.match(query).hasMatch() && !str.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
            qCDebug(KLEOPATRA_LOG) << "Adding 0x prefix to query";
            startKeyListJob(OpenPGP, QStringLiteral("0x") + str);
        } else {
            startKeyListJob(OpenPGP, str);
#ifdef QGPGME_SUPPORTS_WKDLOOKUP
            if (str.contains(QLatin1Char{'@'}) && !searchTextToEmailAddress(str).isEmpty()) {
                startWKDLookupJob(str);
            }
#endif
        }
    }
}

void LookupCertificatesCommand::Private::startKeyListJob(GpgME::Protocol proto, const QString &str)
{
    KeyListJob *const klj = createKeyListJob(proto);
    if (!klj) {
        return;
    }
    connect(klj, &QGpgME::KeyListJob::result, q, [this](const GpgME::KeyListResult &result) { slotKeyListResult(result); });
    connect(klj, &QGpgME::KeyListJob::nextKey, q, [this](const GpgME::Key &key) { slotNextKey(key); });
    if (const Error err = klj->start(QStringList(str))) {
        keyListing.result.mergeWith(KeyListResult(err));
    } else if (proto == CMS) {
        keyListing.cms     = klj;
    } else {
        keyListing.openpgp = klj;
    }
}

#ifdef QGPGME_SUPPORTS_WKDLOOKUP
void LookupCertificatesCommand::Private::startWKDLookupJob(const QString &str)
{
    const auto job = createWKDLookupJob();
    if (!job) {
        qCDebug(KLEOPATRA_LOG) << "Failed to create WKDLookupJob";
        return;
    }
    connect(job, &WKDLookupJob::result,
            q, [this](const WKDLookupResult &result) { slotWKDLookupResult(result); });
    if (const Error err = job->start(str)) {
        keyListing.result.mergeWith(KeyListResult{err});
    } else {
        keyListing.wkdJob = job;
    }
}
#endif

void LookupCertificatesCommand::Private::slotNextKey(const Key &key)
{
    if (!key.primaryFingerprint()) {
        qCDebug(KLEOPATRA_LOG) << __func__ << "ignoring key without fingerprint" << key;
        if (q->sender() == keyListing.cms) {
            keyListing.cmsKeysHaveNoFingerprints = true;
        } else if (q->sender() == keyListing.openpgp) {
            keyListing.openPgpKeysHaveNoFingerprints = true;
        }
    } else if (key.numUserIDs() == 0) {
        qCDebug(KLEOPATRA_LOG) << __func__ << "ignoring key without user IDs" << key;
        keyListing.numKeysWithoutUserId++;
    } else {
        qCDebug(KLEOPATRA_LOG) << __func__ << "got key" << key;
        keyListing.keys.push_back(key);
    }
}

void LookupCertificatesCommand::Private::slotKeyListResult(const KeyListResult &r)
{
    if (q->sender() == keyListing.cms) {
        keyListing.cms = nullptr;
    } else if (q->sender() == keyListing.openpgp) {
        keyListing.openpgp = nullptr;
    } else {
        qCDebug(KLEOPATRA_LOG) << "unknown sender()" << q->sender();
    }

    keyListing.result.mergeWith(r);

    tryToFinishKeyLookup();
}

#ifdef QGPGME_SUPPORTS_WKDLOOKUP
static auto removeKeysNotMatchingEmail(const std::vector<Key> &keys, const std::string &email)
{
    std::vector<Key> filteredKeys;

    const auto addrSpec = UserID::addrSpecFromString(email.c_str());
    std::copy_if(std::begin(keys), std::end(keys),
                 std::back_inserter(filteredKeys),
                 [addrSpec](const auto &key) {
                     const auto uids = key.userIDs();
                     return std::any_of(std::begin(uids), std::end(uids),
                                        [addrSpec](const auto &uid) {
                                            return uid.addrSpec() == addrSpec;
                                        });
                 });

    return filteredKeys;
}

void LookupCertificatesCommand::Private::slotWKDLookupResult(const WKDLookupResult &result)
{
    if (q->sender() == keyListing.wkdJob) {
        keyListing.wkdJob = nullptr;
    } else {
        qCDebug(KLEOPATRA_LOG) << __func__ << "unknown sender()" << q->sender();
    }

    keyListing.result.mergeWith(KeyListResult{result.error()});

    const auto keys = removeKeysNotMatchingEmail(result.keyData().toKeys(GpgME::OpenPGP), result.pattern());
    if (!keys.empty()) {
        keyListing.wkdKeyData = QByteArray::fromStdString(result.keyData().toString());
        keyListing.wkdSource = QString::fromStdString(result.source());
        std::copy(std::begin(keys), std::end(keys),
                  std::back_inserter(keyListing.keys));
        // remember the keys retrieved via WKD for import
        std::transform(std::begin(keys), std::end(keys),
                       std::inserter(keyListing.wkdKeyFingerprints, std::begin(keyListing.wkdKeyFingerprints)),
                       [](const auto &k) { return k.primaryFingerprint(); });
    }

    tryToFinishKeyLookup();
}
#endif

namespace
{
void showKeysWithoutFingerprintsNotification(QWidget *parent, GpgME::Protocol protocol)
{
    if (protocol != GpgME::CMS && protocol != GpgME::OpenPGP) {
        return;
    }

    QString message;
    if (protocol == GpgME::CMS) {
        message = xi18nc("@info",
                         "<para>One of the X.509 directory services returned certificates without "
                         "fingerprints. Those certificates are ignored because fingerprints "
                         "are required as unique identifiers for certificates.</para>"
                         "<para>You may want to configure a different X.509 directory service "
                         "in the configuration dialog.</para>");
    } else {
        message = xi18nc("@info",
                         "<para>The OpenPGP keyserver returned certificates without "
                         "fingerprints. Those certificates are ignored because fingerprints "
                         "are required as unique identifiers for certificates.</para>"
                         "<para>You may want to configure a different OpenPGP keyserver "
                         "in the configuration dialog.</para>");
    }
    KMessageBox::information(parent, message, i18nc("@title", "Invalid Server Reply"),
                             QStringLiteral("certificates-lookup-missing-fingerprints"));
}
}

void LookupCertificatesCommand::Private::tryToFinishKeyLookup()
{
    if (keyListing.cms || keyListing.openpgp
#ifdef QGPGME_SUPPORTS_WKDLOOKUP
        || keyListing.wkdJob
#endif
    ) {
        // still waiting for jobs to complete
        return;
    }

    if (keyListing.result.error() && !keyListing.result.error().isCanceled()) {
        showError(dialog, keyListing.result);
    }

    if (keyListing.result.isTruncated()) {
        showResult(dialog, keyListing.result);
    }

    if (keyListing.cmsKeysHaveNoFingerprints) {
        showKeysWithoutFingerprintsNotification(dialog, GpgME::CMS);
    }
    if (keyListing.openPgpKeysHaveNoFingerprints) {
        showKeysWithoutFingerprintsNotification(dialog, GpgME::OpenPGP);
    }

    if (dialog) {
        dialog->setPassive(false);
        dialog->setCertificates(keyListing.keys);
        if (keyListing.numKeysWithoutUserId > 0) {
            dialog->showInformation(i18ncp("@info",
                                           "One certificate without name and email address was ignored.",
                                           "%1 certificates without name and email address were ignored.",
                                           keyListing.numKeysWithoutUserId));
        }
    } else {
        finished();
    }
}

void LookupCertificatesCommand::Private::slotImportRequested(const std::vector<Key> &keys)
{
    dialog = nullptr;

    Q_ASSERT(!keys.empty());
    Q_ASSERT(std::none_of(keys.cbegin(), keys.cend(), [](const Key &key) { return key.isNull(); }));

    std::vector<Key> wkdKeys, otherKeys;
    otherKeys.reserve(keys.size());
    kdtools::separate_if(std::begin(keys), std::end(keys),
                         std::back_inserter(wkdKeys),
                         std::back_inserter(otherKeys),
                         [this](const auto &key) {
                             return key.primaryFingerprint() &&
                                    keyListing.wkdKeyFingerprints.find(key.primaryFingerprint()) != std::end(keyListing.wkdKeyFingerprints);
                         });

    std::vector<Key> pgp, cms;
    pgp.reserve(otherKeys.size());
    cms.reserve(otherKeys.size());
    kdtools::separate_if(otherKeys.begin(), otherKeys.end(),
                         std::back_inserter(pgp),
                         std::back_inserter(cms),
                         [](const Key &key) {
                             return key.protocol() == GpgME::OpenPGP;
                         });

    setWaitForMoreJobs(true);
    if (!wkdKeys.empty()) {
        // set an import filter, so that only user IDs matching the email address used for the WKD lookup are imported
        const QString importFilter = QLatin1String{"keep-uid=mbox = "} + searchTextToEmailAddress(keyListing.pattern);
        startImport(OpenPGP, keyListing.wkdKeyData, keyListing.wkdSource,
                    {importFilter, Key::OriginWKD, keyListing.wkdSource});
    }
    if (!pgp.empty()) {
        startImport(OpenPGP, pgp,
                    i18nc(R"(@title %1:"OpenPGP" or "S/MIME")",
                          "%1 Certificate Server",
                          Formatting::displayName(OpenPGP)));
    }
    if (!cms.empty()) {
        startImport(CMS, cms,
                    i18nc(R"(@title %1:"OpenPGP" or "S/MIME")",
                          "%1 Certificate Server",
                          Formatting::displayName(CMS)));
    }
    setWaitForMoreJobs(false);
}

void LookupCertificatesCommand::Private::slotSaveAsRequested(const std::vector<Key> &keys)
{
    Q_UNUSED(keys)
    qCDebug(KLEOPATRA_LOG) << "not implemented";
}

void LookupCertificatesCommand::Private::slotDetailsRequested(const Key &key)
{
    Command *const cmd = new DetailsCommand(key, view(), controller());
    cmd->setParentWidget(dialogOrParentWidgetOrView());
    cmd->start();
}

void LookupCertificatesCommand::doCancel()
{
    ImportCertificatesCommand::doCancel();
    if (QDialog *const dlg = d->dialog) {
        d->dialog = nullptr;
        dlg->close();
    }
}

void LookupCertificatesCommand::Private::showError(QWidget *parent, const KeyListResult &result)
{
    if (!result.error()) {
        return;
    }
    KMessageBox::information(parent, i18nc("@info",
                                           "Failed to search on certificate server. The error returned was:\n%1",
                                           QString::fromLocal8Bit(result.error().asString())));
}

void LookupCertificatesCommand::Private::showResult(QWidget *parent, const KeyListResult &result)
{
    if (result.isTruncated())
        KMessageBox::information(parent,
                                 xi18nc("@info",
                                        "<para>The query result has been truncated.</para>"
                                        "<para>Either the local or a remote limit on "
                                        "the maximum number of returned hits has "
                                        "been exceeded.</para>"
                                        "<para>You can try to increase the local limit "
                                        "in the configuration dialog, but if one "
                                        "of the configured servers is the limiting "
                                        "factor, you have to refine your search.</para>"),
                                 i18nc("@title", "Result Truncated"),
                                 QStringLiteral("lookup-certificates-truncated-result"));
}

bool LookupCertificatesCommand::Private::checkConfig() const
{
    const bool haveOrDontNeedOpenPGPServer = haveKeyserverConfigured() || (protocol == GpgME::CMS);
    const bool haveOrDontNeedCMSServer = haveX509DirectoryServerConfigured() || (protocol == GpgME::OpenPGP);
    const bool ok = haveOrDontNeedOpenPGPServer || haveOrDontNeedCMSServer;
    if (!ok)
        information(xi18nc("@info",
                           "<para>You do not have any directory servers configured.</para>"
                           "<para>You need to configure at least one directory server to "
                           "search on one.</para>"
                           "<para>You can configure directory servers here: "
                           "<interface>Settings->Configure Kleopatra</interface>.</para>"),
                    i18nc("@title", "No Directory Servers Configured"));
    return ok;
}

#undef d
#undef q

#include "moc_lookupcertificatescommand.cpp"
