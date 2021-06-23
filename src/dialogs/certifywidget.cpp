/*  dialogs/certifywidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019 g 10code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "certifywidget.h"

#include "kleopatra_debug.h"

#include <KLocalizedString>
#include <KConfigGroup>
#include <KDateComboBox>
#include <KSeparator>
#include <KSharedConfig>

#include <Libkleo/DefaultKeyFilter>
#include <Libkleo/KeySelectionCombo>
#include <Libkleo/Formatting>
#include <Libkleo/KeyCache>
#include <Libkleo/Predicates>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

#include <gpgme++/key.h>

#include <gpgme++/gpgmepp_version.h>
#if GPGMEPP_VERSION >= 0x10E00 // 1.14.0
# define GPGME_HAS_REMARKS
#endif

using namespace Kleo;

namespace {

// Maybe move this in its own file
// based on code from StackOverflow
class AnimatedExpander: public QWidget
{
    Q_OBJECT
public:
    explicit AnimatedExpander(const QString &title = QString(),
                              QWidget *parent = nullptr);
    void setContentLayout(QLayout *contentLayout);

private:
    QGridLayout mainLayout;
    QToolButton toggleButton;
    QFrame headerLine;
    QParallelAnimationGroup toggleAnimation;
    QScrollArea contentArea;
    int animationDuration{300};
};

AnimatedExpander::AnimatedExpander(const QString &title, QWidget *parent):
    QWidget(parent)
{
    toggleButton.setStyleSheet(QStringLiteral("QToolButton { border: none; }"));
    toggleButton.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggleButton.setArrowType(Qt::ArrowType::RightArrow);
    toggleButton.setText(title);
    toggleButton.setCheckable(true);
    toggleButton.setChecked(false);

    headerLine.setFrameShape(QFrame::HLine);
    headerLine.setFrameShadow(QFrame::Sunken);
    headerLine.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    contentArea.setStyleSheet(QStringLiteral("QScrollArea { border: none; }"));
    contentArea.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // start out collapsed
    contentArea.setMaximumHeight(0);
    contentArea.setMinimumHeight(0);

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
        toggleButton.setArrowType(checked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
        toggleAnimation.setDirection(checked ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
        toggleAnimation.start();
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

class SecKeyFilter: public DefaultKeyFilter
{
public:
    SecKeyFilter() : DefaultKeyFilter()
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

class UserIDModel : public QStandardItemModel
{
    Q_OBJECT
public:
    enum Role {
        UserIDIndex = Qt::UserRole
    };
    explicit UserIDModel(QObject *parent = nullptr) : QStandardItemModel(parent) {}

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
        for (const auto &uid: key.userIDs()) {
            if (uid.isRevoked() || uid.isInvalid()) {
                // Skip user ID's that cannot really be certified.
                i++;
                continue;
            }
            auto const item = new QStandardItem;
            item->setText(Formatting::prettyUserID(uid));
            item->setData(i, UserIDIndex);
            item->setCheckable(true);
            item->setEditable(false);
            item->setCheckState(Qt::Checked);
            appendRow(item);
            i++;
        }
    }

    void setCheckedUserIDs(const std::vector<unsigned int> &uids)
    {
        std::vector<unsigned int> sorted = uids;
        std::sort(sorted.begin(), sorted.end());
        for (int i = 0, end = rowCount(); i != end; ++i) {
            item(i)->setCheckState(std::binary_search(sorted.begin(), sorted.end(), i) ? Qt::Checked : Qt::Unchecked);
        }
    }

    std::vector<unsigned int> checkedUserIDs() const
    {
        std::vector<unsigned int> ids;
        for (int i = 0; i < rowCount(); ++i) {
            if (item(i)->checkState() == Qt::Checked) {
                ids.push_back(item(i)->data(UserIDIndex).toUInt());
            }
        }
        qCDebug(KLEOPATRA_LOG) << "Checked uids are: " << ids;
        return ids;
    }

private:
    GpgME::Key m_key;
};

static bool uidEqual(const GpgME::UserID &lhs, const GpgME::UserID &rhs)
{
    return qstrcmp(lhs.parent().primaryFingerprint(),
                    rhs.parent().primaryFingerprint()) == 0
            && qstrcmp(lhs.id(), rhs.id()) == 0;
}

auto checkBoxSize(const QCheckBox *checkBox)
{
    QStyleOptionButton opt;
    return checkBox->style()->sizeFromContents(QStyle::CT_CheckBox, &opt, QSize(), checkBox);
}

auto createInfoButton(const QString &text, QWidget *parent)
{
    auto infoBtn = new QPushButton{parent};
    infoBtn->setIcon(QIcon::fromTheme(QStringLiteral("help-contextual")));
    infoBtn->setFlat(true);

    QObject::connect(infoBtn, &QPushButton::clicked, infoBtn, [infoBtn, text] () {
        QToolTip::showText(infoBtn->mapToGlobal(QPoint()) + QPoint(infoBtn->width(), 0),
                           text, infoBtn, QRect(), 30000);
    });

    return infoBtn;
}

#ifdef QGPGME_SUPPORTS_SIGNATURE_EXPIRATION
QString dateFormatWithFourDigitYear(QLocale::FormatType format)
{
    // Force the year to be formatted as four digit number, so that
    // the user can distinguish between 2006 and 2106.
    return QLocale{}.dateFormat(format).
        replace(QLatin1String("yy"), QLatin1String("yyyy")).
        replace(QLatin1String("yyyyyyyy"), QLatin1String("yyyy"));
}

QString formatDate(const QDate &date, QLocale::FormatType format)
{
    return QLocale{}.toString(date, dateFormatWithFourDigitYear(format));
}
#endif

}

class CertifyWidget::Private
{
public:
    Private(CertifyWidget *qq)
        : q{qq}
        , mFprLabel{new QLabel{q}}
        , mSecKeySelect{new KeySelectionCombo{/* secretOnly= */true, q}}
        , mExportCB{new QCheckBox{q}}
        , mPublishCB{new QCheckBox{q}}
        , mTagsLE{new QLineEdit{q}}
        , mTrustSignatureCB{new QCheckBox{q}}
        , mTrustSignatureDomainLE{new QLineEdit{q}}
        , mExpirationCheckBox{new QCheckBox{q}}
        , mExpirationDateEdit{new KDateComboBox{q}}
    {
        auto mainLay = new QVBoxLayout{q};
        mainLay->addWidget(mFprLabel);

        auto secKeyLay = new QHBoxLayout{q};
        secKeyLay->addWidget(new QLabel(i18n("Certify with:")));

        mSecKeySelect->setKeyFilter(std::make_shared<SecKeyFilter>());

        secKeyLay->addWidget(mSecKeySelect, 1);
        mainLay->addLayout(secKeyLay);

        mainLay->addWidget(new KSeparator{Qt::Horizontal, q});

        auto listView = new QListView{q};
        listView->setModel(&mUserIDModel);
        mainLay->addWidget(listView, 1);

        // Setup the advanced area
        auto expander = new AnimatedExpander{i18n("Advanced"), q};
        mainLay->addWidget(expander);

        auto advLay = new QVBoxLayout{q};

        mExportCB->setText(i18n("Certify for everyone to see (exportable)"));
        advLay->addWidget(mExportCB);

        {
            auto layout = new QHBoxLayout{q};

            mPublishCB->setText(i18n("Publish on keyserver afterwards"));
            mPublishCB->setEnabled(mExportCB->isChecked());

            layout->addSpacing(checkBoxSize(mExportCB).width());
            layout->addWidget(mPublishCB);

            advLay->addLayout(layout);
        }

#ifndef GPGME_HAS_REMARKS
        mTagsLE->setVisible(false);
#else
        {
            auto tagsLay = new QHBoxLayout{q};

            mTagsLE->setPlaceholderText(i18n("Tags"));
            auto infoBtn = createInfoButton(i18n("You can use this to add additional info to a certification.") +
                                            QStringLiteral("<br/><br/>") +
                                            i18n("Tags created by anyone with full certification trust "
                                                "are shown in the keylist and can be searched."),
                                            q);

            tagsLay->addWidget(new QLabel{i18n("Tags:"), q});
            tagsLay->addWidget(mTagsLE, 1);
            tagsLay->addWidget(infoBtn);

            advLay->addLayout(tagsLay);
        }
#endif

#ifndef QGPGME_SUPPORTS_SIGNATURE_EXPIRATION
        mExpirationCheckBox->setVisible(false);
        mExpirationDateEdit->setVisible(false);
#else
        {
            auto layout = new QHBoxLayout{q};

            mExpirationCheckBox->setText(i18n("Expiration:"));

            mExpirationDateEdit->setOptions(KDateComboBox::EditDate | KDateComboBox::SelectDate | KDateComboBox::DatePicker |
                                            KDateComboBox::DateKeywords | KDateComboBox::WarnOnInvalid);
            static const QDate maxAllowedDate{2106, 2, 6};
            const QDate today = QDate::currentDate();
            mExpirationDateEdit->setDateRange(today.addDays(1), maxAllowedDate,
                                              i18n("The certification must be valid at least until tomorrow."),
                                              i18n("The latest allowed certification date is %1.",
                                                   formatDate(maxAllowedDate, QLocale::ShortFormat)));
            mExpirationDateEdit->setDateMap({
                {today.addYears(2), i18nc("Date for expiration of certification", "Two years from now")},
                {today.addYears(1), i18nc("Date for expiration of certification", "One year from now")}
            });
            mExpirationDateEdit->setDate(today.addYears(2));
            mExpirationDateEdit->setEnabled(mExpirationCheckBox->isChecked());

            auto infoBtn = createInfoButton(i18n("You can use this to set an expiration date for a certification.") +
                                            QStringLiteral("<br/><br/>") +
                                            i18n("By setting an expiration date, you can limit the validity of "
                                                 "your certification to a certain amount of time. Once the expiration "
                                                 "date has passed, your certification is no longer valid."),
                                            q);

            layout->addWidget(mExpirationCheckBox);
            layout->addWidget(mExpirationDateEdit, 1);
            layout->addWidget(infoBtn);

            advLay->addLayout(layout);
        }
#endif

#ifndef QGPGME_SUPPORTS_TRUST_SIGNATURES
        mTrustSignatureCB->setVisible(false);
        mTrustSignatureDomainLE->setVisible(false);
#else
        {
            auto layout = new QHBoxLayout{q};

            mTrustSignatureCB->setText(i18n("Certify as trusted introducer"));
            auto infoBtn = createInfoButton(i18n("You can use this to certify a trusted introducer for a domain.") +
                                            QStringLiteral("<br/><br/>") +
                                            i18n("All certificates with email addresses belonging to the domain "
                                                 "that have been certified by the trusted introducer are treated "
                                                 "as certified, i.e. a trusted introducer acts as a kind of "
                                                 "intermediate CA for a domain."),
                                            q);

            layout->addWidget(mTrustSignatureCB, 1);
            layout->addWidget(infoBtn);

            advLay->addLayout(layout);
        }
        {
            auto layout = new QHBoxLayout{q};

            mTrustSignatureDomainLE->setPlaceholderText(i18n("Domain"));
            mTrustSignatureDomainLE->setEnabled(mTrustSignatureCB->isChecked());

            layout->addSpacing(checkBoxSize(mTrustSignatureCB).width());
            layout->addWidget(mTrustSignatureDomainLE);

            advLay->addLayout(layout);
        }
#endif

        expander->setContentLayout(advLay);

        connect(&mUserIDModel, &QStandardItemModel::itemChanged, q, &CertifyWidget::changed);

        connect(mExportCB, &QCheckBox::toggled, [this] (bool on) {
            mPublishCB->setEnabled(on);
        });

        connect(mSecKeySelect, &KeySelectionCombo::currentKeyChanged, [this] (const GpgME::Key &) {
#ifdef GPGME_HAS_REMARKS
            updateTags();
#endif
            Q_EMIT q->changed();
        });

        connect(mExpirationCheckBox, &QCheckBox::toggled, q, [this] (bool checked) {
            mExpirationDateEdit->setEnabled(checked);
            Q_EMIT q->changed();
        });
        connect(mExpirationDateEdit, &KDateComboBox::dateChanged, q, &CertifyWidget::changed);

        connect(mTrustSignatureCB, &QCheckBox::toggled, q, [this] (bool on) {
            mTrustSignatureDomainLE->setEnabled(on);
            Q_EMIT q->changed();
        });
        connect(mTrustSignatureDomainLE, &QLineEdit::textChanged, q, &CertifyWidget::changed);

        loadConfig();
    }

    ~Private() = default;

    void loadConfig()
    {
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
#ifdef GPGME_HAS_REMARKS
        GpgME::Key remarkKey = mSecKeySelect->currentKey();

        if (!remarkKey.isNull()) {
            std::vector<GpgME::UserID> uidsWithRemark;
            QString remark;
            for (const auto &uid: mTarget.userIDs()) {
                GpgME::Error err;
                const char *c_remark = uid.remark(remarkKey, err);
                if (c_remark) {
                    const QString candidate = QString::fromUtf8(c_remark);
                    if (candidate != remark) {
                        qCDebug(KLEOPATRA_LOG) << "Different remarks on user ids. Taking last.";
                        remark = candidate;
                        uidsWithRemark.clear();
                    }
                    uidsWithRemark.push_back(uid);
                }
            }
            // Only select the user ids with the correct remark
            if (!remark.isEmpty()) {
                selectUserIDs(uidsWithRemark);
            }
            mTagsLE->setText(remark);
        }
#endif
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
        mFprLabel->setText(i18n("Fingerprint: <b>%1</b>",
                            Formatting::prettyID(key.primaryFingerprint())) + QStringLiteral("<br/>") +
                            i18n("<i>Only the fingerprint clearly identifies the key and its owner.</i>"));
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
        const auto all = mTarget.userIDs();

        std::vector<unsigned int> indexes;
        indexes.reserve(uids.size());

        for (const auto &uid: uids) {
            const unsigned int idx =
                std::distance(all.cbegin(), std::find_if(all.cbegin(), all.cend(),
                            [uid](const GpgME::UserID &other) { return uidEqual(uid, other); }));
            if (idx < all.size()) {
                indexes.push_back(idx);
            }
        }

        mUserIDModel.setCheckedUserIDs(indexes);
    }

    std::vector<unsigned int> selectedUserIDs() const
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
        // do not accept empty list of user ids
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

public:
    CertifyWidget *const q;
    QLabel *mFprLabel = nullptr;
    KeySelectionCombo *mSecKeySelect = nullptr;
    QCheckBox *mExportCB = nullptr;
    QCheckBox *mPublishCB = nullptr;
    QLineEdit *mTagsLE = nullptr;
    QCheckBox *mTrustSignatureCB = nullptr;
    QLineEdit *mTrustSignatureDomainLE = nullptr;
    QCheckBox *mExpirationCheckBox = nullptr;
    KDateComboBox *mExpirationDateEdit = nullptr;

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

std::vector<unsigned int> CertifyWidget::selectedUserIDs() const
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
