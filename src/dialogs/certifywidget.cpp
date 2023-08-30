/*  dialogs/certifywidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019, 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "certifywidget.h"

#include "view/infofield.h"
#include <utils/accessibility.h>
#include <utils/expiration.h>
#include <utils/keys.h>

#include <settings.h>

#include "kleopatra_debug.h"

#include <KConfigGroup>
#include <KDateComboBox>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KSeparator>
#include <KSharedConfig>

#include <Libkleo/Algorithm>
#include <Libkleo/DefaultKeyFilter>
#include <Libkleo/Formatting>
#include <Libkleo/KeyCache>
#include <Libkleo/KeySelectionCombo>
#include <Libkleo/Predicates>

#include <QGpgME/ChangeOwnerTrustJob>
#include <QGpgME/Protocol>

#include <QAction>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QToolButton>
#include <QVBoxLayout>

#include <gpgme++/key.h>

using namespace Kleo;

static QDebug operator<<(QDebug s, const GpgME::UserID &userID)
{
    return s << Formatting::prettyUserID(userID);
}

namespace
{

// Maybe move this in its own file
// based on code from StackOverflow
class AnimatedExpander : public QWidget
{
    Q_OBJECT
public:
    explicit AnimatedExpander(const QString &title, const QString &accessibleTitle = {}, QWidget *parent = nullptr);
    void setContentLayout(QLayout *contentLayout);

private:
    QGridLayout mainLayout;
    QToolButton toggleButton;
    QFrame headerLine;
    QParallelAnimationGroup toggleAnimation;
    QWidget contentArea;
    int animationDuration{300};
};

AnimatedExpander::AnimatedExpander(const QString &title, const QString &accessibleTitle, QWidget *parent)
    : QWidget{parent}
{
#ifdef Q_OS_WIN
    // draw dotted focus frame if button has focus; otherwise, draw invisible frame using background color
    toggleButton.setStyleSheet(
        QStringLiteral("QToolButton { border: 1px solid palette(window); }"
                       "QToolButton:focus { border: 1px dotted palette(window-text); }"));
#else
    // this works with Breeze style because Breeze draws the focus frame when drawing CE_ToolButtonLabel
    // while the Windows styles (and Qt's common base style) draw the focus frame before drawing CE_ToolButtonLabel
    toggleButton.setStyleSheet(QStringLiteral("QToolButton { border: none; }"));
#endif
    toggleButton.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggleButton.setArrowType(Qt::ArrowType::RightArrow);
    toggleButton.setText(title);
    if (!accessibleTitle.isEmpty()) {
        toggleButton.setAccessibleName(accessibleTitle);
    }
    toggleButton.setCheckable(true);
    toggleButton.setChecked(false);

    headerLine.setFrameShape(QFrame::HLine);
    headerLine.setFrameShadow(QFrame::Sunken);
    headerLine.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    contentArea.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // start out collapsed
    contentArea.setMaximumHeight(0);
    contentArea.setMinimumHeight(0);
    contentArea.setVisible(false);

    // let the entire widget grow and shrink with its content
    toggleAnimation.addAnimation(new QPropertyAnimation(this, "minimumHeight"));
    toggleAnimation.addAnimation(new QPropertyAnimation(this, "maximumHeight"));
    toggleAnimation.addAnimation(new QPropertyAnimation(&contentArea, "maximumHeight"));

    mainLayout.setVerticalSpacing(0);
    mainLayout.setContentsMargins(0, 0, 0, 0);
    int row = 0;
    mainLayout.addWidget(&toggleButton, row, 0, 1, 1, Qt::AlignLeft);
    mainLayout.addWidget(&headerLine, row++, 2, 1, 1);
    mainLayout.addWidget(&contentArea, row, 0, 1, 3);
    setLayout(&mainLayout);
    QObject::connect(&toggleButton, &QToolButton::clicked, [this](const bool checked) {
        if (checked) {
            // make the content visible when expanding starts
            contentArea.setVisible(true);
        }
        toggleButton.setArrowType(checked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
        toggleAnimation.setDirection(checked ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
        toggleAnimation.start();
    });
    connect(&toggleAnimation, &QAbstractAnimation::finished, [this]() {
        // hide the content area when it is fully collapsed
        if (!toggleButton.isChecked()) {
            contentArea.setVisible(false);
        }
    });
}

void AnimatedExpander::setContentLayout(QLayout *contentLayout)
{
    delete contentArea.layout();
    contentArea.setLayout(contentLayout);
    const auto collapsedHeight = sizeHint().height() - contentArea.maximumHeight();
    auto contentHeight = contentLayout->sizeHint().height();
    for (int i = 0; i < toggleAnimation.animationCount() - 1; ++i) {
        auto expanderAnimation = static_cast<QPropertyAnimation *>(toggleAnimation.animationAt(i));
        expanderAnimation->setDuration(animationDuration);
        expanderAnimation->setStartValue(collapsedHeight);
        expanderAnimation->setEndValue(collapsedHeight + contentHeight);
    }
    auto contentAnimation = static_cast<QPropertyAnimation *>(toggleAnimation.animationAt(toggleAnimation.animationCount() - 1));
    contentAnimation->setDuration(animationDuration);
    contentAnimation->setStartValue(0);
    contentAnimation->setEndValue(contentHeight);
}

class SecKeyFilter : public DefaultKeyFilter
{
public:
    SecKeyFilter()
        : DefaultKeyFilter()
    {
        setRevoked(DefaultKeyFilter::NotSet);
        setExpired(DefaultKeyFilter::NotSet);
        setHasSecret(DefaultKeyFilter::Set);
        setCanCertify(DefaultKeyFilter::Set);
        setIsOpenPGP(DefaultKeyFilter::Set);
    }

    bool matches(const GpgME::Key &key, Kleo::KeyFilter::MatchContexts contexts) const override
    {
        if (!(availableMatchContexts() & contexts)) {
            return false;
        }
        if (_detail::ByFingerprint<std::equal_to>()(key, mExcludedKey)) {
            return false;
        }
        return DefaultKeyFilter::matches(key, contexts);
    }

    void setExcludedKey(const GpgME::Key &key)
    {
        mExcludedKey = key;
    }

private:
    GpgME::Key mExcludedKey;
};

class UserIDItem : public QStandardItem
{
public:
    explicit UserIDItem(const GpgME::UserID &uid)
        : mUserId{uid}
    {
    }

    GpgME::UserID userId()
    {
        return mUserId;
    }

private:
    GpgME::UserID mUserId;
};

class UserIDModel : public QStandardItemModel
{
    Q_OBJECT
public:
    enum Role { UserID = Qt::UserRole };
    explicit UserIDModel(QObject *parent = nullptr)
        : QStandardItemModel(parent)
    {
    }

    GpgME::Key certificateToCertify() const
    {
        return m_key;
    }

    void setKey(const GpgME::Key &key)
    {
        m_key = key;
        clear();
        const std::vector<GpgME::UserID> ids = key.userIDs();
        int i = 0;
        for (const auto &uid : key.userIDs()) {
            if (uid.isRevoked() || uid.isInvalid() || Kleo::isRevokedOrExpired(uid)) {
                // Skip user IDs that cannot really be certified.
                i++;
                continue;
            }
            const auto item = new UserIDItem{uid};
            item->setText(Formatting::prettyUserID(uid));
            item->setCheckable(true);
            item->setEditable(false);
            item->setCheckState(Qt::Checked);
            appendRow(item);
            i++;
        }
    }

    void setCheckedUserIDs(const std::vector<GpgME::UserID> &uids)
    {
        for (int i = 0, end = rowCount(); i != end; ++i) {
            const auto uidItem = userIdItem(i);
            const auto itemUserId = uidItem->userId();
            const bool userIdIsInList = Kleo::any_of(uids, [itemUserId](const auto &uid) {
                return Kleo::userIDsAreEqual(itemUserId, uid);
            });
            uidItem->setCheckState(userIdIsInList ? Qt::Checked : Qt::Unchecked);
        }
    }

    std::vector<GpgME::UserID> checkedUserIDs() const
    {
        std::vector<GpgME::UserID> userIds;
        userIds.reserve(rowCount());
        for (int i = 0; i < rowCount(); ++i) {
            const auto uidItem = userIdItem(i);
            if (uidItem->checkState() == Qt::Checked) {
                userIds.push_back(uidItem->userId());
            }
        }
        qCDebug(KLEOPATRA_LOG) << "Checked user IDs:" << userIds;
        return userIds;
    }

private:
    UserIDItem *userIdItem(int row) const
    {
        return static_cast<UserIDItem *>(item(row));
    }

private:
    GpgME::Key m_key;
};

auto checkBoxSize(const QCheckBox *checkBox)
{
    QStyleOptionButton opt;
    return checkBox->style()->sizeFromContents(QStyle::CT_CheckBox, &opt, QSize(), checkBox);
}

class ListView : public QListView
{
    Q_OBJECT
public:
    using QListView::QListView;

protected:
    void focusInEvent(QFocusEvent *event) override
    {
        QListView::focusInEvent(event);
        // queue the invokation, so that it happens after the widget itself got focus
        QMetaObject::invokeMethod(this, &ListView::forceAccessibleFocusEventForCurrentItem, Qt::QueuedConnection);
    }

private:
    void forceAccessibleFocusEventForCurrentItem()
    {
        // force Qt to send a focus event for the current item to accessibility
        // tools; otherwise, the user has no idea which item is selected when the
        // list gets keyboard input focus
        const auto current = currentIndex();
        setCurrentIndex({});
        setCurrentIndex(current);
    }
};

}

class CertifyWidget::Private
{
public:
    Private(CertifyWidget *qq)
        : q{qq}
    {
        auto mainLay = new QVBoxLayout{q};

        {
            auto label = new QLabel{i18n("Verify the fingerprint, mark the user IDs you want to certify, "
                                         "and select the key you want to certify the user IDs with.<br>"
                                         "<i>Note: Only the fingerprint clearly identifies the key and its owner.</i>"),
                                    q};
            label->setWordWrap(true);
            labelHelper.addLabel(label);
            mainLay->addWidget(label);
        }

        mainLay->addWidget(new KSeparator{Qt::Horizontal, q});

        {
            auto grid = new QGridLayout;
            grid->setColumnStretch(1, 1);
            int row = -1;

            row++;
            mFprField = std::make_unique<InfoField>(i18n("Fingerprint:"), q);
            grid->addWidget(mFprField->label(), row, 0);
            grid->addLayout(mFprField->layout(), row, 1);

            row++;
            auto label = new QLabel{i18n("Certify with:"), q};
            mSecKeySelect = new KeySelectionCombo{/* secretOnly= */ true, q};
            mSecKeySelect->setKeyFilter(std::make_shared<SecKeyFilter>());
            label->setBuddy(mSecKeySelect);
            grid->addWidget(label, row, 0);
            grid->addWidget(mSecKeySelect);

            mainLay->addLayout(grid);
        }

        mMissingOwnerTrustInfo = new KMessageWidget{q};
        mSetOwnerTrustAction = new QAction{q};
        mSetOwnerTrustAction->setText(i18nc("@action:button", "Set Owner Trust"));
        mSetOwnerTrustAction->setToolTip(i18nc("@info:tooltip",
                                               "Click to set the trust level of the selected certification key to ultimate trust. "
                                               "This is what you usually want to do for your own keys."));
        connect(mSetOwnerTrustAction, &QAction::triggered, q, [this]() {
            setOwnerTrust();
        });
        mMissingOwnerTrustInfo->addAction(mSetOwnerTrustAction);
        mMissingOwnerTrustInfo->setVisible(false);

        mainLay->addWidget(mMissingOwnerTrustInfo);

        mainLay->addWidget(new KSeparator{Qt::Horizontal, q});

        userIdListView = new ListView{q};
        userIdListView->setAccessibleName(i18n("User IDs"));
        userIdListView->setModel(&mUserIDModel);
        mainLay->addWidget(userIdListView, 1);

        // Setup the advanced area
        auto expander = new AnimatedExpander{i18n("Advanced"), i18n("Show advanced options"), q};
        mainLay->addWidget(expander);

        auto advLay = new QVBoxLayout;

        mExportCB = new QCheckBox{q};
        mExportCB->setText(i18n("Certify for everyone to see (exportable)"));
        advLay->addWidget(mExportCB);

        {
            auto layout = new QHBoxLayout;

            mPublishCB = new QCheckBox{q};
            mPublishCB->setText(i18n("Publish on keyserver afterwards"));
            mPublishCB->setEnabled(mExportCB->isChecked());

            layout->addSpacing(checkBoxSize(mExportCB).width());
            layout->addWidget(mPublishCB);

            advLay->addLayout(layout);
        }

        {
            auto tagsLay = new QHBoxLayout;

            auto label = new QLabel{i18n("Tags:"), q};
            mTagsLE = new QLineEdit{q};
            label->setBuddy(mTagsLE);

            const auto tooltip = i18n("You can use this to add additional info to a certification.") + QStringLiteral("<br/><br/>")
                + i18n("Tags created by anyone with full certification trust "
                       "are shown in the keylist and can be searched.");
            label->setToolTip(tooltip);
            mTagsLE->setToolTip(tooltip);

            tagsLay->addWidget(label);
            tagsLay->addWidget(mTagsLE, 1);

            advLay->addLayout(tagsLay);
        }

        {
            auto layout = new QHBoxLayout;

            mExpirationCheckBox = new QCheckBox{q};
            mExpirationCheckBox->setText(i18n("Expiration:"));

            mExpirationDateEdit = new KDateComboBox{q};
            Kleo::setUpExpirationDateComboBox(mExpirationDateEdit, {QDate::currentDate().addDays(1), QDate{}});
            mExpirationDateEdit->setDate(Kleo::defaultExpirationDate(ExpirationOnUnlimitedValidity::InternalDefaultExpiration));
            mExpirationDateEdit->setEnabled(mExpirationCheckBox->isChecked());

            const auto tooltip = i18n("You can use this to set an expiration date for a certification.") + QStringLiteral("<br/><br/>")
                + i18n("By setting an expiration date, you can limit the validity of "
                       "your certification to a certain amount of time. Once the expiration "
                       "date has passed, your certification is no longer valid.");
            mExpirationCheckBox->setToolTip(tooltip);
            mExpirationDateEdit->setToolTip(tooltip);

            layout->addWidget(mExpirationCheckBox);
            layout->addWidget(mExpirationDateEdit, 1);

            advLay->addLayout(layout);
        }

        {
            mTrustSignatureCB = new QCheckBox{q};
            mTrustSignatureCB->setText(i18n("Certify as trusted introducer"));
            const auto tooltip = i18n("You can use this to certify a trusted introducer for a domain.") + QStringLiteral("<br/><br/>")
                + i18n("All certificates with email addresses belonging to the domain "
                       "that have been certified by the trusted introducer are treated "
                       "as certified, i.e. a trusted introducer acts as a kind of "
                       "intermediate CA for a domain.");
            mTrustSignatureCB->setToolTip(tooltip);

            advLay->addWidget(mTrustSignatureCB);
        }
        {
            auto layout = new QHBoxLayout;

            auto label = new QLabel{i18n("Domain:"), q};

            mTrustSignatureDomainLE = new QLineEdit{q};
            mTrustSignatureDomainLE->setEnabled(mTrustSignatureCB->isChecked());
            label->setBuddy(mTrustSignatureDomainLE);

            layout->addSpacing(checkBoxSize(mTrustSignatureCB).width());
            layout->addWidget(label);
            layout->addWidget(mTrustSignatureDomainLE);

            advLay->addLayout(layout);
        }

        expander->setContentLayout(advLay);

        connect(&mUserIDModel, &QStandardItemModel::itemChanged, q, [this](QStandardItem *item) {
            onItemChanged(item);
        });

        connect(mExportCB, &QCheckBox::toggled, [this](bool on) {
            mPublishCB->setEnabled(on);
        });

        connect(mSecKeySelect, &KeySelectionCombo::currentKeyChanged, [this](const GpgME::Key &) {
            updateTags();
            checkOwnerTrust();
            Q_EMIT q->changed();
        });

        connect(mExpirationCheckBox, &QCheckBox::toggled, q, [this](bool checked) {
            mExpirationDateEdit->setEnabled(checked);
            Q_EMIT q->changed();
        });
        connect(mExpirationDateEdit, &KDateComboBox::dateChanged, q, &CertifyWidget::changed);

        connect(mTrustSignatureCB, &QCheckBox::toggled, q, [this](bool on) {
            mTrustSignatureDomainLE->setEnabled(on);
            Q_EMIT q->changed();
        });
        connect(mTrustSignatureDomainLE, &QLineEdit::textChanged, q, &CertifyWidget::changed);

        loadConfig();
    }

    ~Private() = default;

    void loadConfig()
    {
        const Settings settings;
        mExpirationCheckBox->setChecked(settings.certificationValidityInDays() > 0);
        if (settings.certificationValidityInDays() > 0) {
            const QDate expirationDate = QDate::currentDate().addDays(settings.certificationValidityInDays());
            mExpirationDateEdit->setDate(expirationDate > mExpirationDateEdit->maximumDate() //
                                             ? mExpirationDateEdit->maximumDate() //
                                             : expirationDate);
        }

        const KConfigGroup conf(KSharedConfig::openConfig(), "CertifySettings");
        mSecKeySelect->setDefaultKey(conf.readEntry("LastKey", QString()));
        mExportCB->setChecked(conf.readEntry("ExportCheckState", false));
        mPublishCB->setChecked(conf.readEntry("PublishCheckState", false));
    }

    void updateTags()
    {
        if (mTagsLE->isModified()) {
            return;
        }
        GpgME::Key remarkKey = mSecKeySelect->currentKey();

        if (!remarkKey.isNull()) {
            std::vector<GpgME::UserID> uidsWithRemark;
            QString remark;
            for (const auto &uid : mTarget.userIDs()) {
                GpgME::Error err;
                const char *c_remark = uid.remark(remarkKey, err);
                if (c_remark) {
                    const QString candidate = QString::fromUtf8(c_remark);
                    if (candidate != remark) {
                        qCDebug(KLEOPATRA_LOG) << "Different remarks on user IDs. Taking last.";
                        remark = candidate;
                        uidsWithRemark.clear();
                    }
                    uidsWithRemark.push_back(uid);
                }
            }
            // Only select the user IDs with the correct remark
            if (!remark.isEmpty()) {
                selectUserIDs(uidsWithRemark);
            }
            mTagsLE->setText(remark);
        }
    }

    void updateTrustSignatureDomain()
    {
        if (mTrustSignatureDomainLE->text().isEmpty() && mTarget.numUserIDs() == 1) {
            // try to guess the domain to use for the trust signature
            const auto address = mTarget.userID(0).addrSpec();
            const auto atPos = address.find('@');
            if (atPos != std::string::npos) {
                const auto domain = address.substr(atPos + 1);
                mTrustSignatureDomainLE->setText(QString::fromUtf8(domain.c_str(), domain.size()));
            }
        }
    }

    void setTarget(const GpgME::Key &key)
    {
        mFprField->setValue(QStringLiteral("<b>") + Formatting::prettyID(key.primaryFingerprint()) + QStringLiteral("</b>"),
                            Formatting::accessibleHexID(key.primaryFingerprint()));
        mUserIDModel.setKey(key);
        mTarget = key;

        auto keyFilter = std::make_shared<SecKeyFilter>();
        keyFilter->setExcludedKey(mTarget);
        mSecKeySelect->setKeyFilter(keyFilter);

        updateTags();
        updateTrustSignatureDomain();
    }

    GpgME::Key secKey() const
    {
        return mSecKeySelect->currentKey();
    }

    void selectUserIDs(const std::vector<GpgME::UserID> &uids)
    {
        mUserIDModel.setCheckedUserIDs(uids);
    }

    std::vector<GpgME::UserID> selectedUserIDs() const
    {
        return mUserIDModel.checkedUserIDs();
    }

    bool exportableSelected() const
    {
        return mExportCB->isChecked();
    }

    bool publishSelected() const
    {
        return mPublishCB->isChecked();
    }

    QString tags() const
    {
        return mTagsLE->text().trimmed();
    }

    GpgME::Key target() const
    {
        return mTarget;
    }

    bool isValid() const
    {
        static const QRegularExpression domainNameRegExp{QStringLiteral(R"(^\s*((xn--)?[a-z0-9]+(-[a-z0-9]+)*\.)+[a-z]{2,}\s*$)"),
                                                         QRegularExpression::CaseInsensitiveOption};

        // do not accept null keys
        if (mTarget.isNull() || mSecKeySelect->currentKey().isNull()) {
            return false;
        }
        // do not accept empty list of user IDs
        if (selectedUserIDs().empty()) {
            return false;
        }
        // do not accept if the key to certify is selected as certification key;
        // this shouldn't happen because the key to certify is excluded from the choice, but better safe than sorry
        if (_detail::ByFingerprint<std::equal_to>()(mTarget, mSecKeySelect->currentKey())) {
            return false;
        }
        if (mExpirationCheckBox->isChecked() && !mExpirationDateEdit->isValid()) {
            return false;
        }
        if (mTrustSignatureCB->isChecked() && !domainNameRegExp.match(mTrustSignatureDomainLE->text()).hasMatch()) {
            return false;
        }
        return true;
    }

    void checkOwnerTrust()
    {
        const auto secretKey = secKey();
        if (secretKey.ownerTrust() != GpgME::Key::Ultimate) {
            mMissingOwnerTrustInfo->setMessageType(KMessageWidget::Information);
            mMissingOwnerTrustInfo->setIcon(QIcon::fromTheme(QStringLiteral("question")));
            mMissingOwnerTrustInfo->setText(i18n("Is this your own key?"));
            mSetOwnerTrustAction->setEnabled(true);
            mMissingOwnerTrustInfo->animatedShow();
        } else {
            mMissingOwnerTrustInfo->animatedHide();
        }
    }

    void setOwnerTrust()
    {
        mSetOwnerTrustAction->setEnabled(false);
        QGpgME::ChangeOwnerTrustJob *const j = QGpgME::openpgp()->changeOwnerTrustJob();
        connect(j, &QGpgME::ChangeOwnerTrustJob::result, q, [this](const GpgME::Error &err) {
            if (err) {
                KMessageBox::error(q,
                                   i18n("<p>Changing the certification trust of the key <b>%1</b> failed:</p><p>%2</p>",
                                        Formatting::formatForComboBox(secKey()),
                                        Formatting::errorAsString(err)),
                                   i18n("Certification Trust Change Failed"));
            }
            if (err || err.isCanceled()) {
                mSetOwnerTrustAction->setEnabled(true);
            } else {
                mMissingOwnerTrustInfo->setMessageType(KMessageWidget::Positive);
                mMissingOwnerTrustInfo->setIcon(QIcon::fromTheme(QStringLiteral("checkmark")));
                mMissingOwnerTrustInfo->setText(i18n("Owner trust set successfully."));
            }
        });
        j->start(secKey(), GpgME::Key::Ultimate);
    }

    void onItemChanged(QStandardItem *item)
    {
        Q_EMIT q->changed();

#ifndef QT_NO_ACCESSIBILITY
        if (item) {
            // assume that the checked state changed
            QAccessible::State st;
            st.checked = true;
            QAccessibleStateChangeEvent e(userIdListView, st);
            e.setChild(item->index().row());
            QAccessible::updateAccessibility(&e);
        }
#endif
    }

public:
    CertifyWidget *const q;
    std::unique_ptr<InfoField> mFprField;
    KeySelectionCombo *mSecKeySelect = nullptr;
    KMessageWidget *mMissingOwnerTrustInfo = nullptr;
    ListView *userIdListView = nullptr;
    QCheckBox *mExportCB = nullptr;
    QCheckBox *mPublishCB = nullptr;
    QLineEdit *mTagsLE = nullptr;
    QCheckBox *mTrustSignatureCB = nullptr;
    QLineEdit *mTrustSignatureDomainLE = nullptr;
    QCheckBox *mExpirationCheckBox = nullptr;
    KDateComboBox *mExpirationDateEdit = nullptr;
    QAction *mSetOwnerTrustAction = nullptr;

    LabelHelper labelHelper;

    UserIDModel mUserIDModel;
    GpgME::Key mTarget;
};

CertifyWidget::CertifyWidget(QWidget *parent)
    : QWidget{parent}
    , d{std::make_unique<Private>(this)}
{
}

Kleo::CertifyWidget::~CertifyWidget() = default;

void CertifyWidget::setTarget(const GpgME::Key &key)
{
    d->setTarget(key);
}

GpgME::Key CertifyWidget::target() const
{
    return d->target();
}

void CertifyWidget::selectUserIDs(const std::vector<GpgME::UserID> &uids)
{
    d->selectUserIDs(uids);
}

std::vector<GpgME::UserID> CertifyWidget::selectedUserIDs() const
{
    return d->selectedUserIDs();
}

GpgME::Key CertifyWidget::secKey() const
{
    return d->secKey();
}

bool CertifyWidget::exportableSelected() const
{
    return d->exportableSelected();
}

QString CertifyWidget::tags() const
{
    return d->tags();
}

bool CertifyWidget::publishSelected() const
{
    return d->publishSelected();
}

bool CertifyWidget::trustSignatureSelected() const
{
    return d->mTrustSignatureCB->isChecked();
}

QString CertifyWidget::trustSignatureDomain() const
{
    return d->mTrustSignatureDomainLE->text().trimmed();
}

QDate CertifyWidget::expirationDate() const
{
    return d->mExpirationCheckBox->isChecked() ? d->mExpirationDateEdit->date() : QDate{};
}

bool CertifyWidget::isValid() const
{
    return d->isValid();
}

// For UserID model
#include "certifywidget.moc"
