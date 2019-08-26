/* -*- mode: c++; c-basic-offset:4 -*-
    commands/importcertificatescommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007, 2008 Klarälvdalens Datakonsult AB
    Copyright (c) 2016 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

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

#include "importcertificatescommand.h"
#include "importcertificatescommand_p.h"
#include "certifycertificatecommand.h"
#include "kleopatra_debug.h"

#include <Libkleo/KeyListSortFilterProxyModel>
#include <Libkleo/KeyCache>
#include <Libkleo/Predicates>
#include <Libkleo/Formatting>
#include <Libkleo/Stl_Util>

#include <QGpgME/KeyListJob>
#include <QGpgME/Protocol>
#include <QGpgME/ImportJob>
#include <QGpgME/ImportFromKeyserverJob>
#include <QGpgME/ChangeOwnerTrustJob>

#include <gpgme++/global.h>
#include <gpgme++/importresult.h>
#include <gpgme++/context.h>
#include <gpgme++/gpgmepp_version.h>
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

namespace
{

make_comparator_str(ByImportFingerprint, .fingerprint());

class ImportResultProxyModel : public AbstractKeyListSortFilterProxyModel
{
    Q_OBJECT
public:
    ImportResultProxyModel(const std::vector<ImportResult> &results, const QStringList &ids, QObject *parent = nullptr)
        : AbstractKeyListSortFilterProxyModel(parent)
    {
        updateFindCache(results, ids);
    }

    ~ImportResultProxyModel() override {}

    ImportResultProxyModel *clone() const override
    {
        // compiler-generated copy ctor is fine!
        return new ImportResultProxyModel(*this);
    }

    void setImportResults(const std::vector<ImportResult> &results, const QStringList &ids)
    {
        updateFindCache(results, ids);
        invalidateFilter();
    }

protected:
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || role != Qt::ToolTipRole) {
            return AbstractKeyListSortFilterProxyModel::data(index, role);
        }
        const QString fpr = index.data(FingerprintRole).toString();
        // find information:
        const std::vector<Import>::const_iterator it
            = qBinaryFind(m_importsByFingerprint.begin(), m_importsByFingerprint.end(),
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
        const QString fpr = index.data(FingerprintRole).toString();

        return std::binary_search(m_importsByFingerprint.begin(), m_importsByFingerprint.end(),
                                  fpr.toLatin1().constData(),
                                  ByImportFingerprint<std::less>());
    }

private:
    void updateFindCache(const std::vector<ImportResult> &results, const QStringList &ids)
    {
        Q_ASSERT(results.size() == static_cast<unsigned>(ids.size()));
        m_importsByFingerprint.clear();
        m_idsByFingerprint.clear();
        m_results = results;
        for (unsigned int i = 0, end = results.size(); i != end; ++i) {
            const std::vector<Import> imports = results[i].imports();
            m_importsByFingerprint.insert(m_importsByFingerprint.end(), imports.begin(), imports.end());
            const QString &id = ids[i];
            for (std::vector<Import>::const_iterator it = imports.begin(), end = imports.end(); it != end; ++it) {
                m_idsByFingerprint[it->fingerprint()].insert(id);
            }
        }
        std::sort(m_importsByFingerprint.begin(), m_importsByFingerprint.end(),
                  ByImportFingerprint<std::less>());
    }

private:
    mutable std::vector<Import> m_importsByFingerprint;
    mutable std::map< const char *, std::set<QString>, ByImportFingerprint<std::less> > m_idsByFingerprint;
    std::vector<ImportResult> m_results;
};

}

ImportCertificatesCommand::Private::Private(ImportCertificatesCommand *qq, KeyListController *c)
    : Command::Private(qq, c),
      waitForMoreJobs(false),
      containedExternalCMSCerts(false),
      nonWorkingProtocols(),
      idsByJob(),
      jobs(),
      results(),
      ids()
{

}

ImportCertificatesCommand::Private::~Private() {}

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

ImportCertificatesCommand::~ImportCertificatesCommand() {}

static QString format_ids(const QStringList &ids)
{
    QStringList escapedIds;
    for (const QString &id : ids) {
        if (!id.isEmpty()) {
            escapedIds << id.toHtmlEscaped();
        }
    }
    return escapedIds.join(QLatin1String("<br>"));
}

static QString make_tooltip(const QStringList &ids)
{
    if (ids.empty()) {
        return QString();
    }
    if (ids.size() == 1)
        if (ids.front().isEmpty()) {
            return QString();
        } else
            return i18nc("@info:tooltip",
                         "Imported Certificates from %1",
                         ids.front().toHtmlEscaped());
    else
        return i18nc("@info:tooltip",
                     "Imported certificates from these sources:<br/>%1",
                     format_ids(ids));
}

void ImportCertificatesCommand::Private::setImportResultProxyModel(const std::vector<ImportResult> &results, const QStringList &ids)
{
    if (std::none_of(results.cbegin(), results.cend(), std::mem_fn(&ImportResult::numConsidered))) {
        return;
    }
    q->addTemporaryView(i18nc("@title:tab", "Imported Certificates"),
                        new ImportResultProxyModel(results, ids),
                        make_tooltip(ids));
    if (QTreeView *const tv = qobject_cast<QTreeView *>(parentWidgetOrView())) {
        tv->expandAll();
    }
}

int sum(const std::vector<ImportResult> &res, int (ImportResult::*fun)() const)
{
    return kdtools::accumulate_transform(res.begin(), res.end(), std::mem_fn(fun), 0);
}

static QString make_report(const std::vector<ImportResult> &res, const QString &id = QString())
{

    const KLocalizedString normalLine = ki18n("<tr><td align=\"right\">%1</td><td>%2</td></tr>");
    const KLocalizedString boldLine = ki18n("<tr><td align=\"right\"><b>%1</b></td><td>%2</td></tr>");
    const KLocalizedString headerLine = ki18n("<tr><th colspan=\"2\" align=\"center\">%1</th></tr>");

#define SUM( x ) sum( res, &ImportResult::x )

    QStringList lines;
    if (!id.isEmpty()) {
        lines.push_back(headerLine.subs(id).toString());
    }
    lines.push_back(normalLine.subs(i18n("Total number processed:"))
                    .subs(SUM(numConsidered)).toString());
    lines.push_back(normalLine.subs(i18n("Imported:"))
                    .subs(SUM(numImported)).toString());
    if (const int n = SUM(newSignatures))
        lines.push_back(normalLine.subs(i18n("New signatures:"))
                        .subs(n).toString());
    if (const int n = SUM(newUserIDs))
        lines.push_back(normalLine.subs(i18n("New user IDs:"))
                        .subs(n).toString());
    if (const int n = SUM(numKeysWithoutUserID))
        lines.push_back(normalLine.subs(i18n("Certificates without user IDs:"))
                        .subs(n).toString());
    if (const int n = SUM(newSubkeys))
        lines.push_back(normalLine.subs(i18n("New subkeys:"))
                        .subs(n).toString());
    if (const int n = SUM(newRevocations))
        lines.push_back(boldLine.subs(i18n("Newly revoked:"))
                        .subs(n).toString());
    if (const int n = SUM(notImported))
        lines.push_back(boldLine.subs(i18n("Not imported:"))
                        .subs(n).toString());
    if (const int n = SUM(numUnchanged))
        lines.push_back(normalLine.subs(i18n("Unchanged:"))
                        .subs(n).toString());
    if (const int n = SUM(numSecretKeysConsidered))
        lines.push_back(normalLine.subs(i18n("Secret keys processed:"))
                        .subs(n).toString());
    if (const int n = SUM(numSecretKeysImported))
        lines.push_back(normalLine.subs(i18n("Secret keys imported:"))
                        .subs(n).toString());
    if (const int n = SUM(numSecretKeysConsidered) - SUM(numSecretKeysImported) - SUM(numSecretKeysUnchanged))
        if (n > 0)
            lines.push_back(boldLine.subs(i18n("Secret keys <em>not</em> imported:"))
                            .subs(n).toString());
    if (const int n = SUM(numSecretKeysUnchanged))
        lines.push_back(normalLine.subs(i18n("Secret keys unchanged:"))
                        .subs(n).toString());


#if GPGMEPP_VERSION > 0x10A00 // 1.10.0
    if (const int n = SUM(numV3KeysSkipped))
        lines.push_back(normalLine.subs(i18n("Deprecated PGP-2 keys skipped:"))
                        .subs(n).toString());
#endif


#undef sum

    return lines.join(QString());
}

static QString make_message_report(const std::vector<ImportResult> &res, const QStringList &ids)
{

    Q_ASSERT(res.size() == static_cast<unsigned>(ids.size()));

    if (res.empty()) {
        return i18n("No imports (should not happen, please report a bug).");
    }

    if (res.size() == 1)
        return ids.front().isEmpty()
               ? i18n("<qt><p>Detailed results of certificate import:</p>"
                      "<table width=\"100%\">%1</table></qt>", make_report(res))
               : i18n("<qt><p>Detailed results of importing %1:</p>"
                      "<table width=\"100%\">%2</table></qt>", ids.front(), make_report(res));

    return i18n("<qt><p>Detailed results of certificate import:</p>"
                "<table width=\"100%\">%1</table></qt>", make_report(res, i18n("Totals")));
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

void ImportCertificatesCommand::Private::showDetails(QWidget *parent, const std::vector<ImportResult> &res, const QStringList &ids)
{
    if (res.size() == 1 && res[0].numImported() == 1 && res[0].imports().size() == 1) {
        if (showPleaseCertify(res[0].imports()[0])) {
            return;
        }
    }
    setImportResultProxyModel(res, ids);
    KMessageBox::information(parent, make_message_report(res, ids), i18n("Certificate Import Result"));
}

void ImportCertificatesCommand::Private::showDetails(const std::vector<ImportResult> &res, const QStringList &ids)
{
    showDetails(parentWidgetOrView(), res, ids);
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
    tryToFinish();
}

void ImportCertificatesCommand::Private::importResult(const ImportResult &result)
{

    jobs.erase(std::remove(jobs.begin(), jobs.end(), q->sender()), jobs.end());

    importResult(result, idsByJob[q->sender()]);
}

void ImportCertificatesCommand::Private::importResult(const ImportResult &result, const QString &id)
{

    results.push_back(result);
    ids.push_back(id);

    tryToFinish();
}

static void handleOwnerTrust(const std::vector<GpgME::ImportResult> &results)
{
    //iterate over all imported certificates
    for (const ImportResult &result : results) {
        //when a new certificate got a secret key
        if (result.numSecretKeysImported() >= 1) {
            const char *fingerPr = result.imports()[0].fingerprint();
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
            uids.reserve(toTrustOwner.userIDs().size());
            Q_FOREACH (const UserID &uid, toTrustOwner.userIDs()) {
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

void ImportCertificatesCommand::Private::handleExternalCMSImports()
{
    QStringList fingerprints;
    // For external CMS Imports we have to manually do a keylist
    // with validation to get the intermediate and root ca imported
    // automatically if trusted-certs and extra-certs are used.
    Q_FOREACH (const ImportResult &result, results) {
        Q_FOREACH (const Import &import, result.imports()) {
            if (!import.fingerprint()) {
                continue;
            }
            fingerprints << QString::fromLatin1(import.fingerprint());
        }
    }

    auto job = QGpgME::smime()->keyListJob(false, true, true);

    // Old connect here because of Windows.
    connect(job, SIGNAL(result(GpgME::KeyListResult,std::vector<GpgME::Key>,QString,GpgME::Error)),
            q, SLOT(keyListDone(GpgME::KeyListResult,std::vector<GpgME::Key>,QString,GpgME::Error)));
    job->start(fingerprints, false);
}

void ImportCertificatesCommand::Private::keyListDone(const GpgME::KeyListResult &,
                                                     const std::vector<GpgME::Key> &keys,
                                                     const QString &, const GpgME::Error&)
{
    KeyCache::mutableInstance()->refresh(keys);
    showDetails(results, ids);

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

    if (std::any_of(results.cbegin(), results.cend(),
                    [](const GpgME::ImportResult &result) {
                        return result.error().code();
                    })) {
        setImportResultProxyModel(results, ids);
        if (std::all_of(results.cbegin(), results.cend(),
                        [](const GpgME::ImportResult &result) {
                            return result.error().isCanceled();
                        })) {
            Q_EMIT q->canceled();
        } else {
            for (unsigned int i = 0, end = results.size(); i != end; ++i)
                if (const Error err = results[i].error()) {
                    showError(err, ids[i]);
                }
        }
    } else {
        if (containedExternalCMSCerts) {
            handleExternalCMSImports();
            // We emit finished and do show details
            // after the keylisting.
            return;
        } else {
            handleOwnerTrust(results);
        }
        showDetails(results, ids);
    }
    finished();
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

void ImportCertificatesCommand::Private::startImport(GpgME::Protocol protocol, const QByteArray &data, const QString &id)
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
        importResult(ImportResult(), id);
        return;
    }

    connect(job.get(), SIGNAL(result(GpgME::ImportResult)),
            q, SLOT(importResult(GpgME::ImportResult)));
    connect(job.get(), &Job::progress,
            q, &Command::progress);
    const GpgME::Error err = job->start(data);
    if (err.code()) {
        importResult(ImportResult(err), id);
    } else {
        jobs.push_back(job.release());
        idsByJob[jobs.back()] = id;
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
        importResult(ImportResult(), id);
        return;
    }

    if (protocol == GpgME::CMS) {
        containedExternalCMSCerts = true;
    }

    connect(job.get(), SIGNAL(result(GpgME::ImportResult)),
            q, SLOT(importResult(GpgME::ImportResult)));
    connect(job.get(), &Job::progress,
            q, &Command::progress);
    const GpgME::Error err = job->start(keys);
    if (err.code()) {
        importResult(ImportResult(err), id);
    } else {
        jobs.push_back(job.release());
        idsByJob[jobs.back()] = id;
    }
}

void ImportCertificatesCommand::doCancel()
{
    std::for_each(d->jobs.begin(), d->jobs.end(), [](Job *job) { job->slotCancel(); });
}

#undef d
#undef q

#include "moc_importcertificatescommand.cpp"
#include "importcertificatescommand.moc"

