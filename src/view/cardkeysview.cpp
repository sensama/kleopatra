/*  view/cardkeysview.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2024 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cardkeysview.h"

#include <tooltippreferences.h>

#include <kleopatra_debug.h>

#include <commands/detailscommand.h>
#include <smartcard/card.h>
#include <smartcard/readerstatus.h>
#include <utils/gui-helper.h>
#include <view/progressoverlay.h>
#include <view/smartcardactions.h>

#include <Libkleo/Debug>
#include <Libkleo/Dn>
#include <Libkleo/Formatting>
#include <Libkleo/KeyCache>
#include <Libkleo/KeyHelpers>
#include <Libkleo/TreeWidget>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QGpgME/KeyListJob>
#include <QGpgME/Protocol>

#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

#include <gpgme++/context.h>
#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>

#include <algorithm>

using namespace GpgME;
using namespace Kleo;
using namespace Kleo::SmartCard;
using namespace Kleo::Commands;
using namespace Qt::Literals::StringLiterals;

static int toolTipOptions()
{
    using namespace Kleo::Formatting;
    static const int validityFlags = Validity | Issuer | ExpiryDates | CertificateUsage;
    static const int ownerFlags = Subject | UserIDs | OwnerTrust;
    static const int detailsFlags = StorageLocation | CertificateType | SerialNumber | Fingerprint;

    const TooltipPreferences prefs;

    int flags = KeyID;
    flags |= prefs.showValidity() ? validityFlags : 0;
    flags |= prefs.showOwnerInformation() ? ownerFlags : 0;
    flags |= prefs.showCertificateDetails() ? detailsFlags : 0;
    return flags;
}

namespace
{
enum ColumnIndex {
    Slot,
    KeyGrip,
    Usage,
    Created,
    Fingerprint,
    Certificate,
    Actions,
};
}

namespace
{
static const int CardKeysWidgetItemType = QTreeWidgetItem::UserType;

class CardKeysWidgetItem : public QTreeWidgetItem
{
public:
    CardKeysWidgetItem(int slotIndex, const std::string &keyRef)
        : QTreeWidgetItem{CardKeysWidgetItemType}
        , mSlotIndex{slotIndex}
        , mKeyRef{keyRef}
    {
    }
    ~CardKeysWidgetItem() override = default;

    int slotIndex() const
    {
        return mSlotIndex;
    }

    const std::string &keyRef() const
    {
        return mKeyRef;
    }

    void setSubkey(const Subkey &subkey)
    {
        mSubkey = subkey;
    }
    const Subkey &subkey() const
    {
        return mSubkey;
    }

private:
    int mSlotIndex;
    std::string mKeyRef;
    Subkey mSubkey;
};
}

static QString cardKeyUsageDisplayName(char c)
{
    switch (c) {
    case 'e':
        return i18n("encryption");
    case 's':
        return i18n("signatures");
    case 'c':
        return i18n("certification");
    case 'a':
        return i18n("authentication");
    default:
        return {};
    };
}

static QStringList cardKeyUsageDisplayNames(const std::string &usage)
{
    QStringList result;
    if (usage == "-") {
        // special case (e.g. for some NetKey keys)
        return result;
    }
    result.reserve(usage.size());
    std::ranges::transform(usage, std::back_inserter(result), &cardKeyUsageDisplayName);
    return result;
}

static std::vector<CardKeysWidgetItem *> getItems(const TreeWidget *treeWidget, int slotIndex)
{
    std::vector<CardKeysWidgetItem *> items;
    for (int i = 0; i < treeWidget->topLevelItemCount(); ++i) {
        auto item = static_cast<CardKeysWidgetItem *>(treeWidget->topLevelItem(i));
        if (item->slotIndex() == slotIndex) {
            items.push_back(item);
        } else if (item->slotIndex() > slotIndex) {
            // the items are sorted by slot index so that we do not have to look further
            break;
        }
    }
    return items;
}

static void updateTreeWidgetItem(CardKeysWidgetItem *item, const KeyPairInfo &keyInfo, const Subkey &subkey, CardKeysView::Options options)
{
    Q_ASSERT(item);
    // slot
    item->setData(Slot, Qt::DisplayRole, QString::number(item->slotIndex() + 1));
    // key grip
    item->setData(KeyGrip, Qt::DisplayRole, Formatting::prettyID(keyInfo.grip.c_str()));
    item->setData(KeyGrip, Qt::AccessibleTextRole, Formatting::accessibleHexID(keyInfo.grip.c_str()));
    // usage
    auto usages = cardKeyUsageDisplayNames(keyInfo.usage);
    if (usages.empty()) {
        item->setData(Usage, Qt::DisplayRole, QString::fromStdString(keyInfo.usage));
        item->setData(Usage, Qt::AccessibleTextRole, i18nc("@info entry in Usage column of a smart card key", "none"));
    } else {
        item->setData(Usage, Qt::DisplayRole, usages.join(i18nc("Separator between words in a list", ", ")));
    }
    // created
    if (!(options & CardKeysView::NoCreated)) {
        item->setData(Created, Qt::DisplayRole, QString::fromStdString(keyInfo.keyTime));
    }
    item->setSubkey(subkey);
    if (subkey.isNull()) {
        // fingerprint
        item->setData(Fingerprint, Qt::DisplayRole, QString{});
        // certificate
        item->setData(Certificate, Qt::DisplayRole, QString{});
    } else {
        // fingerprint
        item->setData(Fingerprint, Qt::DisplayRole, Formatting::prettyID(subkey.fingerprint()));
        item->setData(Fingerprint, Qt::AccessibleTextRole, Formatting::accessibleHexID(subkey.fingerprint()));
        // certificate
        if (subkey.parent().protocol() == GpgME::OpenPGP) {
            item->setData(Certificate, Qt::DisplayRole, Formatting::prettyUserID(subkey.parent().userID(0)));
        } else {
            item->setData(Certificate, Qt::DisplayRole, DN(subkey.parent().userID(0).id()).prettyDN());
        }
        item->setData(Certificate, Qt::ToolTipRole, Formatting::toolTip(subkey.parent(), toolTipOptions()));
    }
}

static std::vector<QAction *> actionsForCardSlot(SmartCard::AppType appType)
{
    switch (appType) {
    case AppType::NetKeyApp:
    case AppType::P15App:
        return SmartCardActions::instance()->actions({u"card_all_show_certificate_details"_s});
    case AppType::OpenPGPApp:
    case AppType::PIVApp:
    case AppType::NoApp:
        break;
    };
    return {};
}

static bool canImportCertificates(const Card *card, const std::vector<std::string> &keyRefsWithoutSMimeCertificate)
{
    switch (card->appType()) {
    case AppType::OpenPGPApp:
        // no S/MIME certificates to learn from OpenPGP cards
        return false;
    case AppType::NetKeyApp:
    case AppType::P15App:
        return !keyRefsWithoutSMimeCertificate.empty();
    case AppType::PIVApp:
        // check whether there are S/MIME certificates for the given card slots
        return std::ranges::any_of(keyRefsWithoutSMimeCertificate, [card](const auto &keyRef) {
            return !card->certificateData(keyRef).empty();
        });
    case AppType::NoApp:
        break;
    }
    return false;
}

static inline int compareByProtocolAndFingerprint(const Subkey &a, const Subkey &b)
{
    if (a.parent().protocol() < b.parent().protocol()) {
        return -1;
    }
    if (a.parent().protocol() > b.parent().protocol()) {
        return 1;
    }
    return qstrcmp(a.fingerprint(), b.fingerprint());
}

static auto getSortedSubkeys(const std::string &keyGrip)
{
    auto subkeys = KeyCache::instance()->findSubkeysByKeyGrip(keyGrip);
    // sort subkeys by protocol and fingerprint to ensure a stable list order
    auto lessByProtocolAndFingerprint = [](const Subkey &a, const Subkey &b) {
        return compareByProtocolAndFingerprint(a, b) < 0;
    };
    std::sort(subkeys.begin(), subkeys.end(), lessByProtocolAndFingerprint);
    return subkeys;
}

CardKeysView::CardKeysView(QWidget *parent, Options options)
    : QWidget{parent}
    , mOptions{options}
{
    auto mainLayout = new QVBoxLayout{this};
    mainLayout->setContentsMargins({});

    // The certificate view
    mTreeWidget = new TreeWidget{this};
    mTreeWidget->setAccessibleName(i18nc("@title", "card keys and certificates"));
    mTreeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    mTreeWidget->setRootIsDecorated(false);
    // set a smaller default column size (default for most styles is 100) so that the Actions column doesn't get too wide by default
    mTreeWidget->header()->setDefaultSectionSize(20);
    mTreeWidget->setHeaderLabels({
        i18nc("@title:column Key slot of a smart card", "Slot"),
        i18nc("@title:column", "Keygrip"),
        i18nc("@title:column", "Usage"),
        i18nc("@title:column", "Created"),
        i18nc("@title:column", "Fingerprint"),
        i18nc("@title:column", "Certificate"),
        u" "_s, // Actions column shouldn't have a header title; set " " to prevent Qt using the column number
    });
    if (mOptions & NoCreated) {
        mTreeWidget->forceColumnHidden(Created);
    }
    mainLayout->addWidget(mTreeWidget);

    if (auto action = SmartCardActions::instance()->action(u"card_all_show_certificate_details"_s)) {
        connect(mTreeWidget, &QAbstractItemView::doubleClicked, action, &QAction::trigger);
    }

    mTreeViewOverlay = new ProgressOverlay{mTreeWidget, this};
    mTreeViewOverlay->hide();

    connect(KeyCache::instance().get(), &KeyCache::keysMayHaveChanged, this, [this]() {
        updateKeyList(nullptr);
    });
}

CardKeysView::~CardKeysView() = default;

void CardKeysView::setCard(const Card *card)
{
    mSerialNumber = card->serialNumber();
    mAppName = card->appName();
    mAppType = card->appType();

    updateKeyList(card);
}

std::string CardKeysView::currentCardSlot() const
{
    if (const CardKeysWidgetItem *current = static_cast<CardKeysWidgetItem *>(mTreeWidget->currentItem())) {
        return current->keyRef();
    }
    return {};
}

Key CardKeysView::currentCertificate() const
{
    if (const CardKeysWidgetItem *current = static_cast<CardKeysWidgetItem *>(mTreeWidget->currentItem())) {
        return current->subkey().parent();
    }
    qCDebug(KLEOPATRA_LOG) << __func__ << "- no current item";
    return {};
}

bool CardKeysView::eventFilter(QObject *obj, QEvent *event)
{
    if ((event->type() == QEvent::FocusOut) //
        && (obj == mTreeWidget->itemWidget(mTreeWidget->currentItem(), Actions))) {
        // workaround for missing update when last actions button loses focus
        mTreeWidget->viewport()->update();
    }

    return QWidget::eventFilter(obj, event);
}

void CardKeysView::updateKeyList(const Card *card)
{
    qCDebug(KLEOPATRA_LOG) << __func__;
    const bool firstSetUp = (mTreeWidget->topLevelItemCount() == 0);

    if (mSerialNumber.empty()) {
        // ignore KeyCache::keysMayHaveChanged signal until the card has been set
        return;
    }

    const auto cardRefHolder = card ? std::shared_ptr<Card>{} : ReaderStatus::instance()->getCard(mSerialNumber, mAppName);
    if (!card) {
        card = cardRefHolder.get();
    }
    if (!card) {
        qCDebug(KLEOPATRA_LOG) << "Failed to find the" << mAppName << "smart card with the serial number" << mSerialNumber;
        return;
    }

    std::vector<std::string> keyRefsWithoutSMimeCertificate;
    const auto cardKeyInfos = card->keyInfos();
    mCertificates.clear();
    mCertificates.reserve(cardKeyInfos.size());
    for (int slotIndex = 0; slotIndex < int(cardKeyInfos.size()); ++slotIndex) {
        const auto &keyInfo = cardKeyInfos[slotIndex];
        bool haveFoundSMimeCertificate = false;
        const auto subkeys = getSortedSubkeys(keyInfo.grip);
        auto items = getItems(mTreeWidget, slotIndex);
        if (subkeys.empty()) {
            if (items.empty()) {
                Q_ASSERT(firstSetUp);
                insertTreeWidgetItem(card, slotIndex, keyInfo, Subkey{});
            } else {
                updateTreeWidgetItem(items.front(), keyInfo, Subkey{}, mOptions);
                for (int i = 1; i < int(items.size()); ++i) {
                    auto item = items.at(i);
                    qCDebug(KLEOPATRA_LOG) << __func__ << "deleting item - slot:" << item->slotIndex() << "certificate:" << item->subkey().parent();
                    delete item;
                }
            }
        } else {
            if (items.empty()) {
                Q_ASSERT(firstSetUp);
                for (const auto &subkey : subkeys) {
                    insertTreeWidgetItem(card, slotIndex, keyInfo, subkey);
                }
            } else if (items.front()->subkey().isNull()) {
                // the second most simple case: slot with no associated subkeys -> slot with one or more associated subkeys
                Q_ASSERT(items.size() == 1);
                updateTreeWidgetItem(items.front(), keyInfo, subkeys.front(), mOptions);
                const int itemIndex = mTreeWidget->indexOfTopLevelItem(items.front());
                for (int i = 1; i < int(subkeys.size()); ++i) {
                    insertTreeWidgetItem(card, slotIndex, keyInfo, subkeys.at(i), itemIndex + i);
                }
            } else {
                // the complicated case; we make use of the known order of the existing items and subkeys
                int i = 0;
                int s = 0;
                while (i < int(items.size()) && s < int(subkeys.size())) {
                    auto item = items.at(i);
                    const Subkey &subkey = subkeys.at(s);
                    const int itemVsSubkey = compareByProtocolAndFingerprint(item->subkey(), subkey);
                    if (itemVsSubkey < 0) {
                        // this subkey is gone
                        qCDebug(KLEOPATRA_LOG) << __func__ << "deleting item - slot:" << item->slotIndex() << "certificate:" << item->subkey().parent();
                        delete item;
                        ++i;
                    } else if (itemVsSubkey == 0) {
                        updateTreeWidgetItem(item, keyInfo, subkey, mOptions);
                        ++i;
                        ++s;
                    } else {
                        // this subkey is new; insert it before the current item
                        const int itemIndex = mTreeWidget->indexOfTopLevelItem(item);
                        insertTreeWidgetItem(card, slotIndex, keyInfo, subkey, itemIndex);
                        ++s;
                    }
                }
                for (; i < int(items.size()); ++i) {
                    auto item = items.at(i);
                    qCDebug(KLEOPATRA_LOG) << __func__ << "deleting item - slot:" << item->slotIndex() << "certificate:" << item->subkey().parent();
                    delete item;
                }
                // insert remaining new subkeys after last item for slotIndex
                int insertIndex = 0;
                while ((insertIndex < mTreeWidget->topLevelItemCount()) //
                       && (static_cast<CardKeysWidgetItem *>(mTreeWidget->topLevelItem(insertIndex))->slotIndex() <= slotIndex)) {
                    ++insertIndex;
                }
                insertIndex -= s;
                for (; s < int(subkeys.size()); ++s) {
                    insertTreeWidgetItem(card, slotIndex, keyInfo, subkeys.at(s), insertIndex + s);
                }
            }
            for (const auto &subkey : subkeys) {
                if (subkey.parent().protocol() == GpgME::CMS) {
                    qCDebug(KLEOPATRA_LOG) << __func__ << "Found S/MIME certificate for card key" << keyInfo.grip << "in cache:" << subkey.parent();
                    haveFoundSMimeCertificate = true;
                    mCertificates.push_back(subkey.parent());
                }
            }
        }
        if (!keyInfo.grip.empty() && !haveFoundSMimeCertificate) {
            qCDebug(KLEOPATRA_LOG) << __func__ << "Did not find an S/MIME certificates for card key" << keyInfo.grip << "in cache";
            keyRefsWithoutSMimeCertificate.push_back(keyInfo.keyRef);
        }
    }

    if (firstSetUp && !mTreeWidget->restoreColumnLayout(u"CardKeysView-"_s + QString::fromStdString(mAppName))) {
        mTreeWidget->header()->resizeSections(QHeaderView::ResizeToContents);
    }

    ensureCertificatesAreValidated();

    if (firstSetUp && canImportCertificates(card, keyRefsWithoutSMimeCertificate)) {
        // the card contains keys we don't know; try to learn them from the card
        learnCard();
    }
}

void CardKeysView::insertTreeWidgetItem(const Card *card, int slotIndex, const KeyPairInfo &keyInfo, const Subkey &subkey, int index)
{
    qCDebug(KLEOPATRA_LOG) << __func__ << "slot:" << slotIndex << "certificate:" << subkey.parent() << "index:" << index;
    if (index == -1) {
        index = mTreeWidget->topLevelItemCount();
    }
    auto item = new CardKeysWidgetItem{slotIndex, keyInfo.keyRef};
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren);

    updateTreeWidgetItem(item, keyInfo, subkey, mOptions);
    mTreeWidget->insertTopLevelItem(index, item);
    auto actionsButton = addActionsButton(item, card->appType());
    if (index == 0) {
        forceSetTabOrder(mTreeWidget, actionsButton);
    } else {
        auto prevActionsButton = mTreeWidget->itemWidget(mTreeWidget->topLevelItem(index - 1), Actions);
        forceSetTabOrder(prevActionsButton, actionsButton);
    }
    actionsButton->installEventFilter(this);
}

QToolButton *CardKeysView::addActionsButton(QTreeWidgetItem *item, SmartCard::AppType appType)
{
    const auto actions = actionsForCardSlot(appType);
    auto button = new QToolButton;
    if (actions.size() == 1) {
        button->setDefaultAction(actions.front());
        // ensure that current item is set to the right item before the action is triggered;
        // interestingly, focus is given to the tree widget instead of the clicked button so that
        // the event filtering of QAbstractItemView doesn't take care of this
        connect(button, &QAbstractButton::pressed, mTreeWidget, [this, item]() {
            mTreeWidget->setCurrentItem(item, Actions);
        });
    } else {
        button->setPopupMode(QToolButton::InstantPopup);
        button->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        button->setAccessibleName(i18nc("@action:button", "Actions"));
        button->setToolTip(i18nc("@info", "Show actions available for this smart card slot"));
        // show the menu *after* the clicked item is set as current item to ensure correct action states
        connect(button, &QAbstractButton::pressed, mTreeWidget, [this, item, button, appType]() {
            mTreeWidget->setCurrentItem(item, Actions);
            QMenu menu{button};
            for (auto action : actionsForCardSlot(appType)) {
                menu.addAction(action);
            }
            button->setMenu(&menu);
            button->showMenu();
            button->setMenu(nullptr);
        });
    }
    mTreeWidget->setItemWidget(item, Actions, button);
    return button;
}

void CardKeysView::ensureCertificatesAreValidated()
{
    if (mCertificates.empty()) {
        return;
    }

    std::vector<GpgME::Key> certificatesToValidate;
    certificatesToValidate.reserve(mCertificates.size());
    std::ranges::copy_if(mCertificates, std::back_inserter(certificatesToValidate), [this](const auto &cert) {
        // don't bother validating certificates that have expired or are otherwise invalid
        return !cert.isBad() && !mValidatedCertificates.contains(cert);
    });
    if (!certificatesToValidate.empty()) {
        startCertificateValidation(certificatesToValidate);
        mValidatedCertificates.insert(certificatesToValidate.cbegin(), certificatesToValidate.cend());
    }
}

void CardKeysView::startCertificateValidation(const std::vector<GpgME::Key> &certificates)
{
    qCDebug(KLEOPATRA_LOG) << __func__ << "Validating certificates" << certificates;
    auto job = std::unique_ptr<QGpgME::KeyListJob>{QGpgME::smime()->keyListJob(false, true, true)};
    auto ctx = QGpgME::Job::context(job.get());
    ctx->addKeyListMode(GpgME::WithSecret);

    connect(job.get(), &QGpgME::KeyListJob::result, this, &CardKeysView::certificateValidationDone);

    job->start(Kleo::getFingerprints(certificates));
    job.release();
}

void CardKeysView::certificateValidationDone(const GpgME::KeyListResult &result, const std::vector<GpgME::Key> &validatedCertificates)
{
    qCDebug(KLEOPATRA_LOG) << __func__ << "certificates:" << validatedCertificates;
    if (result.error()) {
        qCDebug(KLEOPATRA_LOG) << __func__ << "Validating certificates failed:" << result.error();
        return;
    }
    // replace the current certificates with the validated certificates
    for (const auto &validatedCert : validatedCertificates) {
        const auto fpr = validatedCert.primaryFingerprint();
        const auto it = std::find_if(mCertificates.begin(), mCertificates.end(), [fpr](const auto &cert) {
            return !qstrcmp(fpr, cert.primaryFingerprint());
        });
        if (it != mCertificates.end()) {
            *it = validatedCert;
        } else {
            qCDebug(KLEOPATRA_LOG) << __func__ << "Didn't find validated certificate in certificate list:" << validatedCert;
        }
    }
    updateKeyList();
}

void CardKeysView::learnCard()
{
    qCDebug(KLEOPATRA_LOG) << __func__;
    mTreeViewOverlay->setText(i18nc("@info", "Reading certificates from smart card ..."));
    mTreeViewOverlay->showOverlay();
    ReaderStatus::mutableInstance()->learnCards(GpgME::CMS);
    connect(ReaderStatus::instance(), &ReaderStatus::cardsLearned, this, [this]() {
        qCDebug(KLEOPATRA_LOG) << "ReaderStatus::cardsLearned";
        mTreeViewOverlay->hideOverlay();
    });
}

#include "moc_cardkeysview.cpp"
