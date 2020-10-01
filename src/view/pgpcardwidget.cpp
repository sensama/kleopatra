/*  view/pgpcardwiget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pgpcardwidget.h"

#include "kleopatra_debug.h"

#include "commands/changepincommand.h"

#include "smartcard/openpgpcard.h"
#include "smartcard/readerstatus.h"

#include "dialogs/gencardkeydialog.h"
#include "utils/gnupg-helper.h"

#include <QProgressDialog>
#include <QThread>
#include <QScrollArea>
#include <QInputDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <KMessageBox>

#include <Libkleo/KeyCache>
#include <Libkleo/Formatting>

#include <gpgme++/gpgmepp_version.h>
#include <gpgme++/data.h>
#include <gpgme++/context.h>

#include <QGpgME/DataProvider>

#include <gpgme++/gpggencardkeyinteractor.h>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::SmartCard;

namespace {
class GenKeyThread: public QThread
{
    Q_OBJECT

    public:
        explicit GenKeyThread(const GenCardKeyDialog::KeyParams &params, const std::string &serial):
            mSerial(serial),
            mParams(params)
        {
        }

        GpgME::Error error()
        {
            return mErr;
        }

        std::string bkpFile()
        {
            return mBkpFile;
        }
    protected:
        void run() override {
            GpgME::GpgGenCardKeyInteractor *ei = new GpgME::GpgGenCardKeyInteractor(mSerial);
            ei->setAlgo(GpgME::GpgGenCardKeyInteractor::RSA);
            ei->setKeySize(QByteArray::fromStdString(mParams.algorithm).toInt());
            ei->setNameUtf8(mParams.name.toStdString());
            ei->setEmailUtf8(mParams.email.toStdString());
            ei->setDoBackup(mParams.backup);

            const auto ctx = std::shared_ptr<GpgME::Context> (GpgME::Context::createForProtocol(GpgME::OpenPGP));
            QGpgME::QByteArrayDataProvider dp;
            GpgME::Data data(&dp);

            mErr = ctx->cardEdit(GpgME::Key(), std::unique_ptr<GpgME::EditInteractor> (ei), data);
            mBkpFile = ei->backupFileName();
        }

    private:
        GpgME::Error mErr;
        std::string mSerial;
        GenCardKeyDialog::KeyParams mParams;

        std::string mBkpFile;
};
} // Namespace

PGPCardWidget::PGPCardWidget(QWidget *parent):
    QWidget(parent),
    mSerialNumber(new QLabel(this)),
    mCardHolderLabel(new QLabel(this)),
    mVersionLabel(new QLabel(this)),
    mSigningKey(new QLabel(this)),
    mEncryptionKey(new QLabel(this)),
    mAuthKey(new QLabel(this)),
    mUrlLabel(new QLabel(this)),
    mCardIsEmpty(false)
{
    auto grid = new QGridLayout;
    int row = 0;

    // Set up the scroll are
    auto area = new QScrollArea;
    area->setFrameShape(QFrame::NoFrame);
    area->setWidgetResizable(true);
    auto areaWidget = new QWidget;
    auto areaVLay = new QVBoxLayout(areaWidget);
    areaVLay->addLayout(grid);
    areaVLay->addStretch(1);
    area->setWidget(areaWidget);
    auto myLayout = new QVBoxLayout(this);
    myLayout->addWidget(area);

    // Version and Serialnumber
    grid->addWidget(mVersionLabel, row++, 0, 1, 2);
    mVersionLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    grid->addWidget(new QLabel(i18n("Serial number:")), row, 0);

    grid->addWidget(mSerialNumber, row++, 1);
    mSerialNumber->setTextInteractionFlags(Qt::TextBrowserInteraction);

    // Cardholder Row
    grid->addWidget(new QLabel(i18nc("The owner of a smartcard. GnuPG refers to this as cardholder.",
                    "Cardholder:")), row, 0);

    grid->addWidget(mCardHolderLabel, row, 1);
    mCardHolderLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    auto nameButtton = new QPushButton;
    nameButtton->setIcon(QIcon::fromTheme(QStringLiteral("cell_edit")));
    nameButtton->setToolTip(i18n("Change"));
    grid->addWidget(nameButtton, row++, 2);
    connect(nameButtton, &QPushButton::clicked, this, &PGPCardWidget::changeNameRequested);

    // URL Row
    grid->addWidget(new QLabel(i18nc("The URL under which a public key that "
                                     "corresponds to a smartcard can be downloaded",
                                     "Pubkey URL:")), row, 0);
    grid->addWidget(mUrlLabel, row, 1);

    mUrlLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    auto urlButtton = new QPushButton;
    urlButtton->setIcon(QIcon::fromTheme(QStringLiteral("cell_edit")));
    urlButtton->setToolTip(i18n("Change"));
    grid->addWidget(urlButtton, row++, 2);
    connect(urlButtton, &QPushButton::clicked, this, &PGPCardWidget::changeUrlRequested);

    // The keys
    auto line1 = new QFrame();
    line1->setFrameShape(QFrame::HLine);
    grid->addWidget(line1, row++, 0, 1, 4);
    grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(i18n("Keys:"))), row++, 0);

    grid->addWidget(new QLabel(i18n("Signature:")), row, 0);
    grid->addWidget(mSigningKey, row++, 1);
    mSigningKey->setTextInteractionFlags(Qt::TextBrowserInteraction);

    grid->addWidget(new QLabel(i18n("Encryption:")), row, 0);
    grid->addWidget(mEncryptionKey, row++, 1);
    mEncryptionKey->setTextInteractionFlags(Qt::TextBrowserInteraction);

    grid->addWidget(new QLabel(i18n("Authentication:")), row, 0);
    grid->addWidget(mAuthKey, row++, 1);
    mAuthKey->setTextInteractionFlags(Qt::TextBrowserInteraction);

    auto line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    grid->addWidget(line2, row++, 0, 1, 4);
    grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(i18n("Actions:"))), row++, 0);

    auto actionLayout = new QHBoxLayout;

    auto generateButton = new QPushButton(i18n("Generate new Keys"));
    generateButton->setToolTip(i18n("Create a new primary key and generate subkeys on the card."));
    actionLayout->addWidget(generateButton);
    connect(generateButton, &QPushButton::clicked, this, &PGPCardWidget::genkeyRequested);

    auto pinButtton = new QPushButton(i18n("Change PIN"));
    pinButtton->setToolTip(i18n("Change the PIN required to unblock the smartcard."));
    actionLayout->addWidget(pinButtton);
    connect(pinButtton, &QPushButton::clicked, this, [this] () { doChangePin(OpenPGPCard::pinKeyRef()); });

    auto pukButton = new QPushButton(i18n("Change Admin PIN"));
    pukButton->setToolTip(i18n("Change the PIN required to unlock the smartcard."));
    actionLayout->addWidget(pukButton);
    connect(pukButton, &QPushButton::clicked, this, [this] () { doChangePin(OpenPGPCard::adminPinKeyRef()); });

    auto resetCodeButton = new QPushButton(i18n("Change Reset Code"));
    pukButton->setToolTip(i18n("Change the PIN required to reset the smartcard to an empty state."));
    actionLayout->addWidget(resetCodeButton);
    connect(resetCodeButton, &QPushButton::clicked, this, [this] () { doChangePin(OpenPGPCard::resetCodeKeyRef()); });

    actionLayout->addStretch(-1);
    grid->addLayout(actionLayout, row++, 0, 1, 4);

    grid->setColumnStretch(4, -1);
}

void PGPCardWidget::setCard(const OpenPGPCard *card)
{
    const QString version = card->displayAppVersion();

    mIs21 = card->appVersion() >= 0x0201;
    const QString manufacturer = QString::fromStdString(card->manufacturer());
    const bool manufacturerIsUnknown = manufacturer.isEmpty() || manufacturer == QLatin1String("unknown");
    mVersionLabel->setText(manufacturerIsUnknown ?
        i18nc("Placeholder is a version number", "Unknown OpenPGP v%1 card", version) :
        i18nc("First placeholder is manufacturer, second placeholder is a version number",
              "%1 OpenPGP v%2 card", manufacturer, version));
    const QString sn = QString::fromStdString(card->serialNumber()).mid(16, 12);
    mSerialNumber->setText(sn);
    mRealSerial = card->serialNumber();

    const auto holder = QString::fromStdString(card->cardHolder());
    const auto url = QString::fromStdString(card->pubkeyUrl());
    mCardHolderLabel->setText(holder.isEmpty() ? i18n("not set") : holder);
    mUrl = url;
    mUrlLabel->setText(url.isEmpty() ? i18n("not set") :
                       QStringLiteral("<a href=\"%1\">%1</a>").arg(url.toHtmlEscaped()));
    mUrlLabel->setOpenExternalLinks(true);

    updateKey(mSigningKey, card->sigFpr());
    updateKey(mEncryptionKey, card->encFpr());
    updateKey(mAuthKey, card->authFpr());
    mCardIsEmpty = card->authFpr().empty() && card->sigFpr().empty() && card->encFpr().empty();
}

void PGPCardWidget::doChangePin(const std::string &keyRef)
{
    auto cmd = new ChangePinCommand(mRealSerial, OpenPGPCard::AppName, this);
    this->setEnabled(false);
    connect(cmd, &ChangePinCommand::finished,
            this, [this]() {
                this->setEnabled(true);
            });
    cmd->setKeyRef(keyRef);
    cmd->start();
}

void PGPCardWidget::doGenKey(GenCardKeyDialog *dlg)
{
    const auto params = dlg->getKeyParams();

    auto progress = new QProgressDialog(this, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::Dialog);
    progress->setAutoClose(true);
    progress->setMinimumDuration(0);
    progress->setMaximum(0);
    progress->setMinimum(0);
    progress->setModal(true);
    progress->setCancelButton(nullptr);
    progress->setWindowTitle(i18nc("@title:window", "Generating Keys"));
    progress->setLabel(new QLabel(i18n("This may take several minutes...")));
    GenKeyThread *workerThread = new GenKeyThread(params, mRealSerial);
    connect(workerThread, &QThread::finished, this, [this, workerThread, progress] {
            progress->accept();
            progress->deleteLater();
            genKeyDone(workerThread->error(), workerThread->bkpFile());
            delete workerThread;
        });
    workerThread->start();
    progress->exec();
}

void PGPCardWidget::genKeyDone(const GpgME::Error &err, const std::string &backup)
{
    if (err) {
        KMessageBox::error(this, i18nc("@info",
                           "Failed to generate new key: %1", QString::fromLatin1(err.asString())),
                           i18nc("@title", "Error"));
        return;
    }
    if (err.isCanceled()) {
        return;
    }
    if (!backup.empty()) {
        const auto bkpFile = QString::fromStdString(backup);
        QFileInfo fi(bkpFile);
        const auto target = QFileDialog::getSaveFileName(this, i18n("Save backup of encryption key"),
                                                         fi.fileName(),
                                                         QStringLiteral("%1 (*.gpg)").arg(i18n("Backup Key")));
        if (!target.isEmpty() && !QFile::copy(bkpFile, target)) {
            KMessageBox::error(this, i18nc("@info",
                               "Failed to move backup. The backup key is still stored under: %1", bkpFile),
                               i18nc("@title", "Error"));
        } else if (!target.isEmpty()) {
            QFile::remove(bkpFile);
        }
    }

    KMessageBox::information(this, i18nc("@info",
                             "Successfully generated a new key for this card."),
                             i18nc("@title", "Success"));
}

void PGPCardWidget::genkeyRequested()
{
    if (!mCardIsEmpty) {
        auto ret = KMessageBox::warningContinueCancel(this,
                i18n("The existing keys on this card will be <b>deleted</b> "
                     "and replaced by new keys.") + QStringLiteral("<br/><br/>") +
                i18n("It will no longer be possible to decrypt past communication "
                     "encrypted for the existing key."),
                i18n("Secret Key Deletion"),
                KStandardGuiItem::guiItem(KStandardGuiItem::Delete),
                KStandardGuiItem::cancel(), QString(), KMessageBox::Notify | KMessageBox::Dangerous);

        if (ret != KMessageBox::Continue) {
            return;
        }
    }

    GenCardKeyDialog *dlg = new GenCardKeyDialog(GenCardKeyDialog::AllKeyAttributes, this);
    std::vector<std::pair<std::string, QString>> algos = {
        { "1024", QStringLiteral("RSA 1024") },
        { "2048", QStringLiteral("RSA 2048") },
        { "3072", QStringLiteral("RSA 3072") }
    };
    // There is probably a better way to check for capabilities
    if (mIs21) {
        algos.push_back({"4096", QStringLiteral("RSA 4096")});
    }
    dlg->setSupportedAlgorithms(algos, "2048");
    connect(dlg, &QDialog::accepted, this, [this, dlg] () {
            doGenKey(dlg);
            dlg->deleteLater();
        });
    dlg->setModal(true);
    dlg->show();
}

void PGPCardWidget::changeNameRequested()
{
    QString text = mCardHolderLabel->text();
    while (true) {
        bool ok = false;
        text = QInputDialog::getText(this, i18n("Change cardholder"),
                                     i18n("New name:"), QLineEdit::Normal,
                                     text, &ok, Qt::WindowFlags(),
                                     Qt::ImhLatinOnly);
        if (!ok) {
            return;
        }
        // Some additional restrictions imposed by gnupg
        if (text.contains(QLatin1Char('<'))) {
            KMessageBox::error(this, i18nc("@info",
                               "The \"<\" character may not be used."),
                               i18nc("@title", "Error"));
            continue;
        }
        if (text.contains(QLatin1String("  "))) {
            KMessageBox::error(this, i18nc("@info",
                               "Double spaces are not allowed"),
                               i18nc("@title", "Error"));
            continue;
        }
        if (text.size() > 38) {
            KMessageBox::error(this, i18nc("@info",
                               "The size of the name may not exceed 38 characters."),
                               i18nc("@title", "Error"));
        }
        break;
    }
    auto parts = text.split(QLatin1Char(' '));
    const auto lastName = parts.takeLast();
    const QString formatted = lastName + QStringLiteral("<<") + parts.join(QLatin1Char('<'));

    const auto pgpCard = ReaderStatus::instance()->getCard<OpenPGPCard>(mRealSerial);
    if (!pgpCard) {
        KMessageBox::error(this, i18n("Failed to find the OpenPGP card with the serial number: %1", QString::fromStdString(mRealSerial)));
        return;
    }

    const QByteArray command = QByteArrayLiteral("SCD SETATTR DISP-NAME ") + formatted.toUtf8();
    ReaderStatus::mutableInstance()->startSimpleTransaction(pgpCard, command, this, "changeNameResult");
}

void PGPCardWidget::changeNameResult(const GpgME::Error &err)
{
    if (err) {
        KMessageBox::error(this, i18nc("@info",
                           "Name change failed: %1", QString::fromLatin1(err.asString())),
                           i18nc("@title", "Error"));
        return;
    }
    if (!err.isCanceled()) {
        KMessageBox::information(this, i18nc("@info",
                    "Name successfully changed."),
                i18nc("@title", "Success"));
        ReaderStatus::mutableInstance()->updateStatus();
    }
}

void PGPCardWidget::changeUrlRequested()
{
    QString text = mUrl;
    while (true) {
        bool ok = false;
        text = QInputDialog::getText(this, i18n("Change the URL where the pubkey can be found"),
                                     i18n("New pubkey URL:"), QLineEdit::Normal,
                                     text, &ok, Qt::WindowFlags(),
                                     Qt::ImhLatinOnly);
        if (!ok) {
            return;
        }
        // Some additional restrictions imposed by gnupg
        if (text.size() > 254) {
            KMessageBox::error(this, i18nc("@info",
                               "The size of the URL may not exceed 254 characters."),
                               i18nc("@title", "Error"));
        }
        break;
    }

    const auto pgpCard = ReaderStatus::instance()->getCard<OpenPGPCard>(mRealSerial);
    if (!pgpCard) {
        KMessageBox::error(this, i18n("Failed to find the OpenPGP card with the serial number: %1", QString::fromStdString(mRealSerial)));
        return;
    }

    const QByteArray command = QByteArrayLiteral("SCD SETATTR PUBKEY-URL ") + text.toUtf8();
    ReaderStatus::mutableInstance()->startSimpleTransaction(pgpCard, command, this, "changeUrlResult");
}

void PGPCardWidget::changeUrlResult(const GpgME::Error &err)
{
    if (err) {
        KMessageBox::error(this, i18nc("@info",
                           "URL change failed: %1", QString::fromLatin1(err.asString())),
                           i18nc("@title", "Error"));
        return;
    }
    if (!err.isCanceled()) {
        KMessageBox::information(this, i18nc("@info",
                    "URL successfully changed."),
                i18nc("@title", "Success"));
        ReaderStatus::mutableInstance()->updateStatus();
    }
}

void PGPCardWidget::updateKey(QLabel *label, const std::string &fpr)
{
    label->setText(QString::fromStdString(fpr));

    if (fpr.empty()) {
        label->setText(i18n("Slot empty"));
        return;
    }

    std::vector<std::string> vec;
    std::string keyid = fpr;
    keyid.erase(0, keyid.size() - 16);
    vec.push_back(keyid);
    const auto subkeys = KeyCache::instance()->findSubkeysByKeyID(vec);
    if (subkeys.empty() || subkeys[0].isNull()) {
        label->setToolTip(i18n("Public key not found."));
        return;
    }
    QStringList toolTips;
    toolTips.reserve(subkeys.size());
    for (const auto &sub: subkeys) {
        // Yep you can have one subkey associated with multiple
        // primary keys.
        toolTips << Formatting::toolTip(sub.parent(), Formatting::Validity |
                                        Formatting::StorageLocation |
                                        Formatting::ExpiryDates |
                                        Formatting::UserIDs |
                                        Formatting::Fingerprint);
    }
    label->setToolTip(toolTips.join(QLatin1String("<br/>")));
    return;
}

#include "pgpcardwidget.moc"
