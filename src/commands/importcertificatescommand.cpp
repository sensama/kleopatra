/* -*- mode: c++; c-basic-offset:4 -*-
    commands/importcertificatescommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007, 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2021, 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "importcertificatescommand.h"
#include "importcertificatescommand_p.h"

#include "certifycertificatecommand.h"
#include "kleopatra_debug.h"

#include <Libkleo/Algorithm>
#include <Libkleo/KeyList>
#include <Libkleo/KeyListSortFilterProxyModel>
#include <Libkleo/KeyCache>
#include <Libkleo/KeyGroupImportExport>
#include <Libkleo/Predicates>
#include <Libkleo/Formatting>
#include <Libkleo/Stl_Util>

#include <QGpgME/KeyListJob>
#include <QGpgME/Protocol>
#include <QGpgME/ImportJob>
#include <QGpgME/ImportFromKeyserverJob>
#include <QGpgME/ChangeOwnerTrustJob>
#ifdef QGPGME_SUPPORTS_RECEIVING_KEYS_BY_KEY_ID
#include <QGpgME/ReceiveKeysJob>
#endif

#include <gpgme++/global.h>
#include <gpgme++/importresult.h>
#include <gpgme++/context.h>
#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>

#include <KLocalizedString>
#include <KMessageBox>

#include <QByteArray>
#include <QEventLoop>
#include <QString>
#include <QWidget>
#include <QTreeView>
#include <QTextDocument> // for Qt::escape

#include <memory>
#include <algorithm>
#include <map>
#include <set>

using namespace GpgME;
using namespace Kleo;
using namespace QGpgME;

bool operator==(const ImportJobData &lhs, const ImportJobData &rhs)
{
    return lhs.job == rhs.job;
}

namespace
{

make_comparator_str(ByImportFingerprint, .fingerprint());

class ImportResultProxyModel : public AbstractKeyListSortFilterProxyModel
{
    Q_OBJECT
public:
    ImportResultProxyModel(const std::vector<ImportResultData> &results, QObject *parent = nullptr)
        : AbstractKeyListSortFilterProxyModel(parent)
    {
        updateFindCache(results);
    }

    ~ImportResultProxyModel() override {}

    ImportResultProxyModel *clone() const override
    {
        // compiler-generated copy ctor is fine!
        return new ImportResultProxyModel(*this);
    }

    void setImportResults(const std::vector<ImportResultData> &results)
    {
        updateFindCache(results);
        invalidateFilter();
    }

protected:
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || role != Qt::ToolTipRole) {
            return AbstractKeyListSortFilterProxyModel::data(index, role);
        }
        const QString fpr = index.data(KeyList::FingerprintRole).toString();
        // find information:
        const std::vector<Import>::const_iterator it
            = Kleo::binary_find(m_importsByFingerprint.begin(), m_importsByFingerprint.end(),
                                fpr.toLatin1().constData(),
                                ByImportFingerprint<std::less>());
        if (it == m_importsByFingerprint.end()) {
            return AbstractKeyListSortFilterProxyModel::data(index, role);
        } else {
            QStringList rv;
            const auto ids = m_idsByFingerprint[it->fingerprint()];
            rv.reserve(ids.size());
            std::copy(ids.cbegin(), ids.cend(), std::back_inserter(rv));
            return Formatting::importMetaData(*it, rv);
        }
    }
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override
    {
        //
        // 0. Keep parents of matching children:
        //
        const QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        Q_ASSERT(index.isValid());
        for (int i = 0, end = sourceModel()->rowCount(index); i != end; ++i)
            if (filterAcceptsRow(i, index)) {
                return true;
            }
        //
        // 1. Check that this is an imported key:
        //
        const QString fpr = index.data(KeyList::FingerprintRole).toString();

        return std::binary_search(m_importsByFingerprint.begin(), m_importsByFingerprint.end(),
                                  fpr.toLatin1().constData(),
                                  ByImportFingerprint<std::less>());
    }

private:
    void updateFindCache(const std::vector<ImportResultData> &results)
    {
        m_importsByFingerprint.clear();
        m_idsByFingerprint.clear();
        m_results = results;
        for (const auto &r : results) {
            const std::vector<Import> imports = r.result.imports();
            m_importsByFingerprint.insert(m_importsByFingerprint.end(), imports.begin(), imports.end());
            for (std::vector<Import>::const_iterator it = imports.begin(), end = imports.end(); it != end; ++it) {
                m_idsByFingerprint[it->fingerprint()].insert(r.id);
            }
        }
        std::sort(m_importsByFingerprint.begin(), m_importsByFingerprint.end(),
                  ByImportFingerprint<std::less>());
    }

private:
    mutable std::vector<Import> m_importsByFingerprint;
    mutable std::map< const char *, std::set<QString>, ByImportFingerprint<std::less> > m_idsByFingerprint;
    std::vector<ImportResultData> m_results;
};

bool importFailed(const ImportResultData &r)
{
    // ignore GPG_ERR_EOF error to handle the "failed" import of files
    // without X.509 certificates by gpgsm gracefully
    return r.result.error() && r.result.error().code() != GPG_ERR_EOF;
}

bool importWasCanceled(const ImportResultData &r)
{
    return r.result.error().isCanceled();
}

}

ImportCertificatesCommand::Private::Private(ImportCertificatesCommand *qq, KeyListController *c)
    : Command::Private(qq, c)
{
}

ImportCertificatesCommand::Private::~Private() = default;

#define d d_func()
#define q q_func()

ImportCertificatesCommand::ImportCertificatesCommand(KeyListController *p)
    : Command(new Private(this, p))
{
}

ImportCertificatesCommand::ImportCertificatesCommand(QAbstractItemView *v, KeyListController *p)
    : Command(v, new Private(this, p))
{
}

ImportCertificatesCommand::~ImportCertificatesCommand() = default;

static QString format_ids(const std::vector<QString> &ids)
{
    QStringList escapedIds;
    for (const QString &id : ids) {
        if (!id.isEmpty()) {
            escapedIds << id.toHtmlEscaped();
        }
    }
    return escapedIds.join(QLatin1String("<br>"));
}

static QString make_tooltip(const std::vector<ImportResultData> &results)
{
    if (results.empty()) {
        return {};
    }

    std::vector<QString> ids;
    ids.reserve(results.size());
    std::transform(std::begin(results), std::end(results),
                   std::back_inserter(ids),
                   [](const auto &r) { return r.id; });
    std::sort(std::begin(ids), std::end(ids));
    ids.erase(std::unique(std::begin(ids), std::end(ids)), std::end(ids));

    if (ids.size() == 1)
        if (ids.front().isEmpty()) {
            return {};
        } else
            return i18nc("@info:tooltip",
                         "Imported Certificates from %1",
                         ids.front().toHtmlEscaped());
    else
        return i18nc("@info:tooltip",
                     "Imported certificates from these sources:<br/>%1",
                     format_ids(ids));
}

void ImportCertificatesCommand::Private::setImportResultProxyModel(const std::vector<ImportResultData> &results)
{
    if (std::none_of(std::begin(results), std::end(results),
                     [](const auto &r) { return r.result.numConsidered() > 0; })) {
        return;
    }
    q->addTemporaryView(i18nc("@title:tab", "Imported Certificates"),
                        new ImportResultProxyModel(results),
                        make_tooltip(results));
    if (QTreeView *const tv = qobject_cast<QTreeView *>(parentWidgetOrView())) {
        tv->expandAll();
    }
}

int sum(const std::vector<ImportResult> &res, int (ImportResult::*fun)() const)
{
    return kdtools::accumulate_transform(res.begin(), res.end(), std::mem_fn(fun), 0);
}

static QString make_report(const std::vector<ImportResultData> &results,
                           const std::vector<ImportedGroup> &groups)
{
    const KLocalizedString normalLine = ki18n("<tr><td align=\"right\">%1</td><td>%2</td></tr>");
    const KLocalizedString boldLine = ki18n("<tr><td align=\"right\"><b>%1</b></td><td>%2</td></tr>");
    const KLocalizedString headerLine = ki18n("<tr><th colspan=\"2\" align=\"center\">%1</th></tr>");

    std::vector<ImportResult> res;
    res.reserve(results.size());
    std::transform(std::begin(results), std::end(results),
                   std::back_inserter(res),
                   [](const auto &r) { return r.result; });

    const auto numProcessedCertificates = sum(res, &ImportResult::numConsidered);

    QStringList lines;

    if (numProcessedCertificates > 0 || groups.size() == 0) {
        lines.push_back(headerLine.subs(i18n("Certificates")).toString());
        lines.push_back(normalLine.subs(i18n("Total number processed:"))
                        .subs(numProcessedCertificates).toString());
        lines.push_back(normalLine.subs(i18n("Imported:"))
                        .subs(sum(res, &ImportResult::numImported)).toString());
        if (const int n = sum(res, &ImportResult::newSignatures))
            lines.push_back(normalLine.subs(i18n("New signatures:"))
                            .subs(n).toString());
        if (const int n = sum(res, &ImportResult::newUserIDs))
            lines.push_back(normalLine.subs(i18n("New user IDs:"))
                            .subs(n).toString());
        if (const int n = sum(res, &ImportResult::numKeysWithoutUserID))
            lines.push_back(normalLine.subs(i18n("Certificates without user IDs:"))
                            .subs(n).toString());
        if (const int n = sum(res, &ImportResult::newSubkeys))
            lines.push_back(normalLine.subs(i18n("New subkeys:"))
                            .subs(n).toString());
        if (const int n = sum(res, &ImportResult::newRevocations))
            lines.push_back(boldLine.subs(i18n("Newly revoked:"))
                            .subs(n).toString());
        if (const int n = sum(res, &ImportResult::notImported))
            lines.push_back(boldLine.subs(i18n("Not imported:"))
                            .subs(n).toString());
        if (const int n = sum(res, &ImportResult::numUnchanged))
            lines.push_back(normalLine.subs(i18n("Unchanged:"))
                            .subs(n).toString());
        if (const int n = sum(res, &ImportResult::numSecretKeysConsidered))
            lines.push_back(normalLine.subs(i18n("Secret keys processed:"))
                            .subs(n).toString());
        if (const int n = sum(res, &ImportResult::numSecretKeysImported))
            lines.push_back(normalLine.subs(i18n("Secret keys imported:"))
                            .subs(n).toString());
        if (const int n = sum(res, &ImportResult::numSecretKeysConsidered) - sum(res, &ImportResult::numSecretKeysImported) - sum(res, &ImportResult::numSecretKeysUnchanged))
            if (n > 0)
                lines.push_back(boldLine.subs(i18n("Secret keys <em>not</em> imported:"))
                                .subs(n).toString());
        if (const int n = sum(res, &ImportResult::numSecretKeysUnchanged))
            lines.push_back(normalLine.subs(i18n("Secret keys unchanged:"))
                            .subs(n).toString());
        if (const int n = sum(res, &ImportResult::numV3KeysSkipped))
            lines.push_back(normalLine.subs(i18n("Deprecated PGP-2 keys skipped:"))
                            .subs(n).toString());
    }

    if (!lines.empty()) {
        lines.push_back(headerLine.subs(QLatin1String{"&nbsp;"}).toString());
    }

    if (groups.size() > 0) {
        const auto newGroups = std::count_if(std::begin(groups), std::end(groups),
                                             [](const auto &g) {
                                                 return g.status == ImportedGroup::Status::New;
                                             });
        const auto updatedGroups = groups.size() - newGroups;
        lines.push_back(headerLine.subs(i18n("Certificate Groups")).toString());
        lines.push_back(normalLine.subs(i18n("Total number processed:"))
                        .subs(groups.size()).toString());
        lines.push_back(normalLine.subs(i18n("New groups:"))
                        .subs(newGroups).toString());
        lines.push_back(normalLine.subs(i18n("Updated groups:"))
                        .subs(updatedGroups).toString());
    }

    return lines.join(QLatin1String{});
}

static QString make_message_report(const std::vector<ImportResultData> &res,
                                   const std::vector<ImportedGroup> &groups)
{
    QString report{QLatin1String{"<html>"}};
    if (res.empty()) {
        report += i18n("No imports (should not happen, please report a bug).");
    } else {
        const bool singleSource = (res.size() == 1) || (res.size() == 2 && res[0].id == res[1].id);
        const QString title = singleSource && !res.front().id.isEmpty() ?
                              i18n("Detailed results of importing %1:", res.front().id) :
                              i18n("Detailed results of import:");
        report += QLatin1String{"<p>"} + title + QLatin1String{"</p>"};
        report += QLatin1String{"<p><table width=\"100%\">"};
        report += make_report(res, groups);
        report += QLatin1String{"</table></p>"};
    }
    report += QLatin1String{"</html>"};
    return report;
}

// Returns false on error, true if please certify was shown.
bool ImportCertificatesCommand::Private::showPleaseCertify(const GpgME::Import &imp)
{
    const char *fpr = imp.fingerprint();
    if (!fpr) {
        // WTF
        qCWarning(KLEOPATRA_LOG) << "Import without fingerprint";
        return false;
    }
    // Exactly one public key imported. Let's see if it is openpgp. We are async here so
    // we can just fetch it.

    auto ctx = GpgME::Context::createForProtocol(GpgME::OpenPGP);
    if (!ctx) {
        // WTF
        qCWarning(KLEOPATRA_LOG) << "Failed to create OpenPGP proto";
        return false;
    }
    GpgME::Error err;
    auto key = ctx->key(fpr, err, false);
    delete ctx;

    if (key.isNull() || err) {
        // No such key most likely not OpenPGP
        return false;
    }

    for (const auto &uid: key.userIDs()) {
        if (uid.validity() >= GpgME::UserID::Marginal) {
            // Already marginal so don't bug the user
            return false;
        }
    }

    const QStringList suggestions = QStringList() << i18n("A phone call to the person.")
        << i18n("Using a business card.")
        << i18n("Confirming it on a trusted website.");

    auto sel = KMessageBox::questionYesNo(parentWidgetOrView(),
                i18n("In order to mark the certificate as valid (green) it needs to be certified.") + QStringLiteral("<br>") +
                i18n("Certifying means that you check the Fingerprint.") + QStringLiteral("<br>") +
                i18n("Some suggestions to do this are:") +
                QStringLiteral("<li><ul>%1").arg(suggestions.join(QStringLiteral("</ul><ul>"))) +
                QStringLiteral("</ul></li>") +
                i18n("Do you wish to start this process now?"),
                i18nc("@title", "You have imported a new certificate (public key)"),
                KStandardGuiItem::yes(), KStandardGuiItem::no(), QStringLiteral("CertifyQuestion"));
    if (sel == KMessageBox::Yes) {
        QEventLoop loop;
        auto cmd = new Commands::CertifyCertificateCommand(key);
        cmd->setParentWidget(parentWidgetOrView());
        loop.connect(cmd, SIGNAL(finished()), SLOT(quit()));
        QMetaObject::invokeMethod(cmd, &Commands::CertifyCertificateCommand::start, Qt::QueuedConnection);
        loop.exec();
    }
    return true;
}

void ImportCertificatesCommand::Private::showDetails(const std::vector<ImportResultData> &res,
                                                     const std::vector<ImportedGroup> &groups)
{
    if (res.size() == 1 && res[0].result.numImported() == 1 && res[0].result.imports().size() == 1) {
        if (showPleaseCertify(res[0].result.imports()[0])) {
            return;
        }
    }
    setImportResultProxyModel(res);
    information(make_message_report(res, groups),
                i18n("Certificate Import Result"));
}

static QString make_error_message(const Error &err, const QString &id)
{
    Q_ASSERT(err);
    Q_ASSERT(!err.isCanceled());
    return id.isEmpty()
           ? i18n("<qt><p>An error occurred while trying "
                  "to import the certificate:</p>"
                  "<p><b>%1</b></p></qt>",
                  QString::fromLocal8Bit(err.asString()))
           : i18n("<qt><p>An error occurred while trying "
                  "to import the certificate %1:</p>"
                  "<p><b>%2</b></p></qt>",
                  id, QString::fromLocal8Bit(err.asString()));
}

void ImportCertificatesCommand::Private::showError(QWidget *parent, const Error &err, const QString &id)
{
    if (parent) {
        KMessageBox::error(parent, make_error_message(err, id), i18n("Certificate Import Failed"));
    } else {
        showError(err, id);
    }
}

void ImportCertificatesCommand::Private::showError(const Error &err, const QString &id)
{
    error(make_error_message(err, id), i18n("Certificate Import Failed"));
}

void ImportCertificatesCommand::Private::setWaitForMoreJobs(bool wait)
{
    if (wait == waitForMoreJobs) {
        return;
    }
    waitForMoreJobs = wait;
    if (!waitForMoreJobs) {
        tryToFinish();
    }
}

void ImportCertificatesCommand::Private::importResult(const ImportResult &result, QGpgME::Job *finishedJob)
{
    if (!finishedJob) {
        finishedJob = qobject_cast<QGpgME::Job *>(q->sender());
    }
    Q_ASSERT(finishedJob);

    auto it = std::find_if(std::cbegin(jobs), std::cend(jobs),
                           [finishedJob](const auto &job) { return job.job == finishedJob; });
    Q_ASSERT(it != std::cend(jobs));
    if (it == std::cend(jobs)) {
        qCWarning(KLEOPATRA_LOG) << __func__ << "Error: Finished job not found";
        return;
    }

    const auto job = *it;
    jobs.erase(std::remove(std::begin(jobs), std::end(jobs), job), std::end(jobs));

    importResult({job.id, job.protocol, job.type, result});
}

void ImportCertificatesCommand::Private::importResult(const ImportResultData &result)
{
    qCDebug(KLEOPATRA_LOG) << __func__ << result.id;
    results.push_back(result);

    tryToFinish();
}

static void handleOwnerTrust(const std::vector<ImportResultData> &results)
{
    //iterate over all imported certificates
    for (const auto &r: results) {
        //when a new certificate got a secret key
        if (r.result.numSecretKeysImported() >= 1) {
            const char *fingerPr = r.result.imports()[0].fingerprint();
            GpgME::Error err;
            QScopedPointer<Context>
                ctx(Context::createForProtocol(GpgME::Protocol::OpenPGP));

            if (!ctx){
                qCWarning(KLEOPATRA_LOG) << "Failed to get context";
                continue;
            }

            const Key toTrustOwner = ctx->key(fingerPr, err , false);

            if (toTrustOwner.isNull()) {
                return;
            }

            QStringList uids;
            const auto toTrustOwnerUserIDs{toTrustOwner.userIDs()};
            uids.reserve(toTrustOwnerUserIDs.size());
            for (const UserID &uid : toTrustOwnerUserIDs) {
                uids << Formatting::prettyNameAndEMail(uid);
            }

            const QString str = xi18nc("@info",
                "<title>You have imported a Secret Key.</title>"
                "<para>The key has the fingerprint:<nl/>"
                "<numid>%1</numid>"
                "</para>"
                "<para>And claims the User IDs:"
                "<list><item>%2</item></list>"
                "</para>"
                "Is this your own key? (Set trust level to ultimate)",
                QString::fromUtf8(fingerPr),
                uids.join(QLatin1String("</item><item>")));

            int k = KMessageBox::questionYesNo(nullptr, str, i18nc("@title:window",
                                                               "Secret key imported"));

            if (k == KMessageBox::Yes) {
                //To use the ChangeOwnerTrustJob over
                //the CryptoBackendFactory
                const QGpgME::Protocol *const backend = QGpgME::openpgp();

                if (!backend){
                    qCWarning(KLEOPATRA_LOG) << "Failed to get CryptoBackend";
                    return;
                }

                ChangeOwnerTrustJob *const j = backend->changeOwnerTrustJob();
                j->start(toTrustOwner, Key::Ultimate);
            }
        }
    }
}

static void validateImportedCertificate(const GpgME::Import &import)
{
    if (const auto fpr = import.fingerprint()) {
        auto key = KeyCache::instance()->findByFingerprint(fpr);
        if (!key.isNull()) {
            // this triggers a keylisting with validation for this certificate
            key.update();
        } else {
            qCWarning(KLEOPATRA_LOG) << __func__ << "Certificate with fingerprint" << fpr << "not found";
        }
    }
}

static void handleExternalCMSImports(const std::vector<ImportResultData> &results)
{
    // For external CMS Imports we have to manually do a keylist
    // with validation to get the intermediate and root ca imported
    // automatically if trusted-certs and extra-certs are used.
    for (const auto &r : results) {
        if (r.protocol == GpgME::CMS && r.type == ImportType::External
                && !importFailed(r) && !importWasCanceled(r)) {
            const auto imports = r.result.imports();
            std::for_each(std::begin(imports), std::end(imports), &validateImportedCertificate);
        }
    }
}

void ImportCertificatesCommand::Private::processResults()
{
    handleExternalCMSImports(results);

    handleOwnerTrust(results);

    importGroups();

    showDetails(results, importedGroups);

    auto tv = dynamic_cast<QTreeView *> (view());
    if (!tv) {
        qCDebug(KLEOPATRA_LOG) << "Failed to find treeview";
    } else {
        tv->expandAll();
    }
    finished();
}

void ImportCertificatesCommand::Private::tryToFinish()
{

    if (waitForMoreJobs || !jobs.empty()) {
        return;
    }

    auto keyCache = KeyCache::mutableInstance();
    keyListConnection = connect(keyCache.get(), &KeyCache::keyListingDone,
                                q, [this]() { keyCacheUpdated(); });
    keyCache->startKeyListing();
}

void ImportCertificatesCommand::Private::keyCacheUpdated()
{
    disconnect(keyListConnection);

    keyCacheAutoRefreshSuspension.reset();

    const auto allIds = std::accumulate(std::cbegin(results), std::cend(results),
                                        std::set<QString>{},
                                        [](auto &allIds, const auto &r) {
                                            allIds.insert(r.id);
                                            return allIds;
                                        });
    const auto canceledIds = std::accumulate(std::cbegin(results), std::cend(results),
                                             std::set<QString>{},
                                             [](auto &canceledIds, const auto &r) {
                                                 if (importWasCanceled(r)) {
                                                     canceledIds.insert(r.id);
                                                 }
                                                 return canceledIds;
                                             });
    const auto totalConsidered = std::accumulate(std::cbegin(results), std::cend(results),
                                                 0,
                                                 [](auto totalConsidered, const auto &r) {
                                                     return totalConsidered + r.result.numConsidered();
                                                 });
    if (totalConsidered == 0 && canceledIds.size() == allIds.size()) {
        // nothing was considered for import and at least one import per id was
        // canceled => treat the command as canceled
        canceled();
        return;
    }

    if (std::any_of(std::cbegin(results), std::cend(results), &importFailed)) {
        setImportResultProxyModel(results);
        for (const auto &r : results) {
            if (importFailed(r)) {
                showError(r.result.error(), r.id);
            }
        }
        finished();
        return;
    }

    processResults();
}

static ImportedGroup storeGroup(const KeyGroup &group, const QString &id)
{
    const auto status = KeyCache::instance()->group(group.id()).isNull() ?
                        ImportedGroup::Status::New :
                        ImportedGroup::Status::Updated;
    if (status == ImportedGroup::Status::New) {
        KeyCache::mutableInstance()->insert(group);
    } else {
        KeyCache::mutableInstance()->update(group);
    }
    return {id, group, status};
}

void ImportCertificatesCommand::Private::importGroups()
{
    for (const auto &path : filesToImportGroupsFrom) {
        const bool certificateImportSucceeded =
            std::any_of(std::cbegin(results), std::cend(results),
                        [path](const auto &r) {
                            return r.id == path && !importFailed(r) && !importWasCanceled(r);
                        });
        if (certificateImportSucceeded) {
            qCDebug(KLEOPATRA_LOG) << __func__ << "Importing groups from file" << path;
            const auto groups = readKeyGroups(path);
            std::transform(std::begin(groups), std::end(groups),
                           std::back_inserter(importedGroups),
                           [path](const auto &group) {
                               return storeGroup(group, path);
                           });
        }
    }
}

static std::unique_ptr<ImportJob> get_import_job(GpgME::Protocol protocol)
{
    Q_ASSERT(protocol != UnknownProtocol);
    if (const auto backend = (protocol == GpgME::OpenPGP ? QGpgME::openpgp() : QGpgME::smime())) {
        return std::unique_ptr<ImportJob>(backend->importJob());
    } else {
        return std::unique_ptr<ImportJob>();
    }
}

void ImportCertificatesCommand::Private::startImport(GpgME::Protocol protocol, const QByteArray &data, const QString &id, const ImportOptions &options)
{
    Q_ASSERT(protocol != UnknownProtocol);

    if (std::find(nonWorkingProtocols.cbegin(), nonWorkingProtocols.cend(), protocol) != nonWorkingProtocols.cend()) {
        return;
    }

    std::unique_ptr<ImportJob> job = get_import_job(protocol);
    if (!job.get()) {
        nonWorkingProtocols.push_back(protocol);
        error(i18n("The type of this certificate (%1) is not supported by this Kleopatra installation.",
                   Formatting::displayName(protocol)),
              i18n("Certificate Import Failed"));
        importResult({id, protocol, ImportType::Local, ImportResult{}});
        return;
    }

    keyCacheAutoRefreshSuspension = KeyCache::mutableInstance()->suspendAutoRefresh();

    std::vector<QMetaObject::Connection> connections = {
        connect(job.get(), SIGNAL(result(GpgME::ImportResult)),
                q, SLOT(importResult(GpgME::ImportResult))),
        connect(job.get(), &Job::progress,
                q, &Command::progress)
    };

#ifdef QGPGME_SUPPORTS_IMPORT_WITH_FILTER
    job->setImportFilter(options.importFilter);
#endif
#ifdef QGPGME_SUPPORTS_IMPORT_WITH_KEY_ORIGIN
    job->setKeyOrigin(options.keyOrigin, options.keyOriginUrl);
#endif
    const GpgME::Error err = job->start(data);
    if (err.code()) {
        importResult({id, protocol, ImportType::Local, ImportResult{err}});
    } else {
        jobs.push_back({id, protocol, ImportType::Local, job.release(), connections});
    }
}

static std::unique_ptr<ImportFromKeyserverJob> get_import_from_keyserver_job(GpgME::Protocol protocol)
{
    Q_ASSERT(protocol != UnknownProtocol);
    if (const auto backend = (protocol == GpgME::OpenPGP ? QGpgME::openpgp() : QGpgME::smime())) {
        return std::unique_ptr<ImportFromKeyserverJob>(backend->importFromKeyserverJob());
    } else {
        return std::unique_ptr<ImportFromKeyserverJob>();
    }
}

void ImportCertificatesCommand::Private::startImport(GpgME::Protocol protocol, const std::vector<Key> &keys, const QString &id)
{
    Q_ASSERT(protocol != UnknownProtocol);

    if (std::find(nonWorkingProtocols.cbegin(), nonWorkingProtocols.cend(), protocol) != nonWorkingProtocols.cend()) {
        return;
    }

    std::unique_ptr<ImportFromKeyserverJob> job = get_import_from_keyserver_job(protocol);
    if (!job.get()) {
        nonWorkingProtocols.push_back(protocol);
        error(i18n("The type of this certificate (%1) is not supported by this Kleopatra installation.",
                   Formatting::displayName(protocol)),
              i18n("Certificate Import Failed"));
        importResult({id, protocol, ImportType::External, ImportResult{}});
        return;
    }

    keyCacheAutoRefreshSuspension = KeyCache::mutableInstance()->suspendAutoRefresh();

    std::vector<QMetaObject::Connection> connections = {
        connect(job.get(), SIGNAL(result(GpgME::ImportResult)),
                q, SLOT(importResult(GpgME::ImportResult))),
        connect(job.get(), &Job::progress,
                q, &Command::progress)
    };

    const GpgME::Error err = job->start(keys);
    if (err.code()) {
        importResult({id, protocol, ImportType::External, ImportResult{err}});
    } else {
        jobs.push_back({id, protocol, ImportType::External, job.release(), connections});
    }
}

static auto get_receive_keys_job(GpgME::Protocol protocol)
{
    Q_ASSERT(protocol != UnknownProtocol);

#ifdef QGPGME_SUPPORTS_RECEIVING_KEYS_BY_KEY_ID
    std::unique_ptr<ReceiveKeysJob> job{};
    if (const auto backend = (protocol == GpgME::OpenPGP ? QGpgME::openpgp() : QGpgME::smime())) {
        job.reset(backend->receiveKeysJob());
    }
    return job;
#else
    return std::unique_ptr<Job>{};
#endif
}

void ImportCertificatesCommand::Private::startImport(GpgME::Protocol protocol, const QStringList &keyIds, const QString &id)
{
    Q_ASSERT(protocol != UnknownProtocol);

    auto job = get_receive_keys_job(protocol);
    if (!job.get()) {
        qCWarning(KLEOPATRA_LOG) << "Failed to get ReceiveKeysJob for protocol" << Formatting::displayName(protocol);
        importResult({id, protocol, ImportType::External, ImportResult{}});
        return;
    }

#ifdef QGPGME_SUPPORTS_RECEIVING_KEYS_BY_KEY_ID
    keyCacheAutoRefreshSuspension = KeyCache::mutableInstance()->suspendAutoRefresh();

    std::vector<QMetaObject::Connection> connections = {
        connect(job.get(), SIGNAL(result(GpgME::ImportResult)),
                q, SLOT(importResult(GpgME::ImportResult))),
        connect(job.get(), &Job::progress,
                q, &Command::progress)
    };

    const GpgME::Error err = job->start(keyIds);
    if (err.code()) {
        importResult({id, protocol, ImportType::External, ImportResult{err}});
    } else {
        jobs.push_back({id, protocol, ImportType::External, job.release(), connections});
    }
#endif
}

void ImportCertificatesCommand::Private::importGroupsFromFile(const QString &filename)
{
    filesToImportGroupsFrom.push_back(filename);
}

void ImportCertificatesCommand::doCancel()
{
    std::for_each(std::cbegin(d->jobs), std::cend(d->jobs),
                  [this](const auto &job) {
                      std::for_each(std::cbegin(job.connections), std::cend(job.connections),
                                    [this](const auto &connection) { QObject::disconnect(connection); });
                      job.job->slotCancel();
                      d->importResult(ImportResult{Error::fromCode(GPG_ERR_CANCELED)}, job.job);
                });
}

#undef d
#undef q

#include "moc_importcertificatescommand.cpp"
#include "importcertificatescommand.moc"

