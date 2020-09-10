/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/resultitemwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "resultitemwidget.h"

#include "utils/auditlog.h"
#include "commands/command.h"
#include "commands/importcertificatefromfilecommand.h"
#include "commands/lookupcertificatescommand.h"
#include "crypto/decryptverifytask.h"

#include <Libkleo/MessageBox>
#include <Libkleo/Classify>

#include <gpgme++/key.h>
#include <gpgme++/gpgmepp_version.h>
#include <gpgme++/decryptionresult.h>

#include <KLocalizedString>
#include <QPushButton>
#include <KStandardGuiItem>
#include "kleopatra_debug.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QUrl>
#include <QVBoxLayout>
#include <KGuiItem>
#include <KColorScheme>

#if GPGMEPP_VERSION > 0x10B01 // > 1.11.1
# define GPGME_HAS_LEGACY_NOMDC
#endif

using namespace Kleo;
using namespace Kleo::Crypto;
using namespace Kleo::Crypto::Gui;

namespace
{
// TODO move out of here
static QColor colorForVisualCode(Task::Result::VisualCode code)
{
    switch (code) {
    case Task::Result::AllGood:
        return KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::PositiveBackground).color();
    case Task::Result::NeutralError:
    case Task::Result::Warning:
        return KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::NormalBackground).color();
    case Task::Result::Danger:
        return KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::NegativeBackground).color();
    case Task::Result::NeutralSuccess:
    default:
        return QColor(0x00, 0x80, 0xFF); // light blue
    }
}
static QColor txtColorForVisualCode(Task::Result::VisualCode code)
{
    switch (code) {
    case Task::Result::AllGood:
        return KColorScheme(QPalette::Active, KColorScheme::View).foreground(KColorScheme::PositiveText).color();
    case Task::Result::NeutralError:
    case Task::Result::Warning:
        return KColorScheme(QPalette::Active, KColorScheme::View).foreground(KColorScheme::NormalText).color();
    case Task::Result::Danger:
        return KColorScheme(QPalette::Active, KColorScheme::View).foreground(KColorScheme::NegativeText).color();
    case Task::Result::NeutralSuccess:
    default:
        return QColor(0xFF, 0xFF, 0xFF); // white
    }
}
}

class ResultItemWidget::Private
{
    ResultItemWidget *const q;
public:
    explicit Private(const std::shared_ptr<const Task::Result> &result, ResultItemWidget *qq) : q(qq), m_result(result)
    {
        Q_ASSERT(m_result);
    }

    void slotLinkActivated(const QString &);
    void updateShowDetailsLabel();

    void addKeyImportButton(QBoxLayout *lay, bool search);
    void addIgnoreMDCButton(QBoxLayout *lay);

    void oneImportFinished();

    const std::shared_ptr<const Task::Result> m_result;
    QLabel *m_detailsLabel = nullptr;
    QLabel *m_actionsLabel = nullptr;
    QPushButton *m_closeButton = nullptr;
    bool m_importCanceled = false;
};

void ResultItemWidget::Private::oneImportFinished()
{
    if (m_importCanceled) {
        return;
    }
    if (m_result->parentTask()) {
        m_result->parentTask()->start();
    }
    q->setVisible(false);
}

void ResultItemWidget::Private::addIgnoreMDCButton(QBoxLayout *lay)
{
    if (!m_result || !lay) {
        return;
    }

    const auto dvResult = dynamic_cast<const DecryptVerifyResult *>(m_result.get());
    if (!dvResult) {
        return;
    }
    const auto decResult = dvResult->decryptionResult();

#ifdef GPGME_HAS_LEGACY_NOMDC
    if (decResult.isNull() || !decResult.error() || !decResult.isLegacyCipherNoMDC())
#endif
    {
        return;
    }

    auto btn = new QPushButton(i18n("Force decryption"));
    btn->setFixedSize(btn->sizeHint());

    connect (btn, &QPushButton::clicked, q, [this] () {
        if (m_result->parentTask()) {
            const auto dvTask = dynamic_cast<DecryptVerifyTask*>(m_result->parentTask().data());
            dvTask->setIgnoreMDCError(true);
            dvTask->start();
            q->setVisible(false);
        } else {
            qCWarning(KLEOPATRA_LOG) << "Failed to get parent task";
        }
    });
    lay->addWidget(btn);
}

void ResultItemWidget::Private::addKeyImportButton(QBoxLayout *lay, bool search)
{
    if (!m_result || !lay) {
        return;
    }

    const auto dvResult = dynamic_cast<const DecryptVerifyResult *>(m_result.get());
    if (!dvResult) {
        return;
    }
    const auto verifyResult = dvResult->verificationResult();

    if (verifyResult.isNull()) {
        return;
    }

    for (const auto &sig: verifyResult.signatures()) {
        if (!(sig.summary() & GpgME::Signature::KeyMissing)) {
            continue;
        }

        auto btn = new QPushButton;
        QString suffix;
        const auto keyid = QLatin1String(sig.fingerprint());
        if (verifyResult.numSignatures() > 1) {
            suffix = QLatin1Char(' ') + keyid;
        }
        btn = new QPushButton(search ? i18nc("1 is optional keyid. No space is intended as it can be empty.",
                                       "Search%1", suffix)
                                     : i18nc("1 is optional keyid. No space is intended as it can be empty.",
                                       "Import%1", suffix));

        if (search) {
            btn->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
            connect (btn, &QPushButton::clicked, q, [this, btn, keyid] () {
                btn->setEnabled(false);
                m_importCanceled = false;
                auto cmd = new Kleo::Commands::LookupCertificatesCommand(keyid, nullptr);
                connect(cmd, &Kleo::Commands::LookupCertificatesCommand::canceled,
                        q, [this]() { m_importCanceled = true; });
                connect(cmd, &Kleo::Commands::LookupCertificatesCommand::finished,
                        q, [this, btn]() {
                    btn->setEnabled(true);
                    oneImportFinished();
                });
                cmd->setParentWidget(q);
                cmd->start();
            });
        } else {
            btn->setIcon(QIcon::fromTheme(QStringLiteral("view-certificate-import")));
            connect (btn, &QPushButton::clicked, q, [this, btn] () {
                btn->setEnabled(false);
                m_importCanceled = false;
                auto cmd = new Kleo::ImportCertificateFromFileCommand();
                connect(cmd, &Kleo::ImportCertificateFromFileCommand::canceled,
                        q, [this]() { m_importCanceled = true; });
                connect(cmd, &Kleo::ImportCertificateFromFileCommand::finished,
                        q, [this, btn]() {
                    btn->setEnabled(true);
                    oneImportFinished();
                });
                cmd->setParentWidget(q);
                cmd->start();
            });
        }
        btn->setFixedSize(btn->sizeHint());
        lay->addWidget(btn);
    }
}

static QUrl auditlog_url_template()
{
    QUrl url(QStringLiteral("kleoresultitem://showauditlog"));
    return url;
}

void ResultItemWidget::Private::updateShowDetailsLabel()
{
    if (!m_actionsLabel || !m_detailsLabel) {
        return;
    }

    const auto parentTask = m_result->parentTask();
    QString auditLogLink;
    if (m_result->hasError()) {
        auditLogLink = m_result->auditLog().formatLink(auditlog_url_template(), i18n("Diagnostics"));
    } else {
        auditLogLink = m_result->auditLog().formatLink(auditlog_url_template());
    }
    m_actionsLabel->setText(auditLogLink);
}

ResultItemWidget::ResultItemWidget(const std::shared_ptr<const Task::Result> &result, QWidget *parent, Qt::WindowFlags flags) : QWidget(parent, flags), d(new Private(result, this))
{
    const QColor color = colorForVisualCode(d->m_result->code());
    const QColor txtColor = txtColorForVisualCode(d->m_result->code());
    const QString styleSheet = QStringLiteral("QFrame,QLabel { background-color: %1; margin: 0px; }"
                                              "QFrame#resultFrame{ border-color: %2; border-style: solid; border-radius: 3px; border-width: 1px }"
                                              "QLabel { color: %3; padding: 5px; border-radius: 3px }").arg(color.name()).arg(color.darker(150).name()).arg(txtColor.name());
    QVBoxLayout *topLayout = new QVBoxLayout(this);
    QFrame *frame = new QFrame;
    frame->setObjectName(QStringLiteral("resultFrame"));
    frame->setStyleSheet(styleSheet);
    topLayout->addWidget(frame);
    QHBoxLayout *layout = new QHBoxLayout(frame);
    QVBoxLayout *vlay = new QVBoxLayout();
    QLabel *overview = new QLabel;
    overview->setWordWrap(true);
    overview->setTextFormat(Qt::RichText);
    overview->setText(d->m_result->overview());
    overview->setFocusPolicy(Qt::StrongFocus);
    overview->setStyleSheet(styleSheet);
    connect(overview, SIGNAL(linkActivated(QString)), this, SLOT(slotLinkActivated(QString)));

    vlay->addWidget(overview);
    layout->addLayout(vlay);

    const QString details = d->m_result->details();

    QVBoxLayout *actionLayout = new QVBoxLayout;
    layout->addLayout(actionLayout);

    d->addKeyImportButton(actionLayout, false);
    // TODO: Only show if auto-key-retrieve is not set.
    d->addKeyImportButton(actionLayout, true);

    d->addIgnoreMDCButton(actionLayout);

    d->m_actionsLabel = new QLabel;
    connect(d->m_actionsLabel, SIGNAL(linkActivated(QString)), this, SLOT(slotLinkActivated(QString)));
    actionLayout->addWidget(d->m_actionsLabel);
    d->m_actionsLabel->setFocusPolicy(Qt::StrongFocus);
    d->m_actionsLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    d->m_actionsLabel->setStyleSheet(styleSheet);

    d->m_detailsLabel = new QLabel;
    d->m_detailsLabel->setWordWrap(true);
    d->m_detailsLabel->setTextFormat(Qt::RichText);
    d->m_detailsLabel->setText(details);
    d->m_detailsLabel->setFocusPolicy(Qt::StrongFocus);
    d->m_detailsLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    d->m_detailsLabel->setStyleSheet(styleSheet);

    connect(d->m_detailsLabel, SIGNAL(linkActivated(QString)), this, SLOT(slotLinkActivated(QString)));
    vlay->addWidget(d->m_detailsLabel);

    d->m_closeButton = new QPushButton;
    KGuiItem::assign(d->m_closeButton, KStandardGuiItem::close());
    d->m_closeButton->setFixedSize(d->m_closeButton->sizeHint());
    connect(d->m_closeButton, &QAbstractButton::clicked, this, &ResultItemWidget::closeButtonClicked);
    actionLayout->addWidget(d->m_closeButton);
    d->m_closeButton->setVisible(false);

    layout->setStretch(0, 1);
    actionLayout->addStretch(-1);
    vlay->addStretch(-1);

    d->updateShowDetailsLabel();
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
}

ResultItemWidget::~ResultItemWidget()
{
}

void ResultItemWidget::showCloseButton(bool show)
{
    d->m_closeButton->setVisible(show);
}

bool ResultItemWidget::hasErrorResult() const
{
    return d->m_result->hasError();
}

void ResultItemWidget::Private::slotLinkActivated(const QString &link)
{
    Q_ASSERT(m_result);
    qCDebug(KLEOPATRA_LOG) << "Link activated: " << link;
    if (link.startsWith(QLatin1String("key:"))) {
        auto split = link.split(QLatin1Char(':'));
        auto fpr = split.value(1);
        if (split.size() == 2 && isFingerprint(fpr)) {
            /* There might be a security consideration here if somehow
             * a short keyid is used in a link and it collides with another.
             * So we additionally check that it really is a fingerprint. */
            auto cmd = Command::commandForQuery(fpr);
            cmd->setParentWId(q->effectiveWinId());
            cmd->start();
        } else {
            qCWarning(KLEOPATRA_LOG) << "key link invalid " << link;
        }
        return;
    }

    const QUrl url(link);

    if (url.host() == QLatin1String("showauditlog")) {
        q->showAuditLog();
        return;
    }
    qCWarning(KLEOPATRA_LOG) << "Unexpected link scheme: " << link;
}

void ResultItemWidget::showAuditLog()
{
    MessageBox::auditLog(parentWidget(), d->m_result->auditLog().text());
}

#include "moc_resultitemwidget.cpp"
