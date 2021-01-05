/*  dialogs/certifywidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019 g 10code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "certifywidget.h"

#include "kleopatra_debug.h"

#include <KLocalizedString>
#include <KConfigGroup>
#include <KSharedConfig>

#include <Libkleo/DefaultKeyFilter>
#include <Libkleo/KeySelectionCombo>
#include <Libkleo/Formatting>
#include <Libkleo/KeyCache>

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
                              const int animationDuration = 300,
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

AnimatedExpander::AnimatedExpander(const QString &title,
                                   const int animationDuration, QWidget *parent):
    QWidget(parent),
    animationDuration(animationDuration)
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
            QStandardItem *const item = new QStandardItem;
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

} // anonymous namespace


// Use of pimpl as this might be moved to libkleo
class CertifyWidget::Private
{
public:
    Private(CertifyWidget *qq) : q(qq),
        mFprLabel(new QLabel)
    {
        QVBoxLayout *mainLay = new QVBoxLayout(q);
        mainLay->addWidget(mFprLabel);

        auto secKeyLay = new QHBoxLayout;
        secKeyLay->addWidget(new QLabel(i18n("Certify with:")));

        mSecKeySelect = new KeySelectionCombo(true);
        mSecKeySelect->setKeyFilter(std::shared_ptr<KeyFilter>(new SecKeyFilter()));

        secKeyLay->addWidget(mSecKeySelect, 1);
        mainLay->addLayout(secKeyLay);

        auto splitLine = new QFrame;
        splitLine->setFrameShape(QFrame::HLine);
        splitLine->setFrameShadow(QFrame::Sunken);
        splitLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

        mainLay->addWidget(splitLine);

        auto listView = new QListView;
        listView->setModel(&mUserIDModel);
        mainLay->addWidget(listView, 1);

        // Setup the advanced area
        auto expander = new AnimatedExpander(i18n("Advanced"));
        mainLay->addWidget(expander);

        auto advLay = new QVBoxLayout;

        mExportCB = new QCheckBox(i18n("Certify for everyone to see. (exportable)"));
        mPublishCB = new QCheckBox(i18n("Publish on keyserver afterwards."));
        auto publishLay = new QHBoxLayout;
        publishLay->addSpacing(20);
        publishLay->addWidget(mPublishCB);

        mTagsLE = new QLineEdit;
        mTagsLE->setPlaceholderText(i18n("Tags"));

        auto infoBtn = new QPushButton;
        infoBtn->setIcon(QIcon::fromTheme(QStringLiteral("help-contextual")));
        infoBtn->setFlat(true);

        connect(infoBtn, &QPushButton::clicked, q, [this, infoBtn] () {
            const QString msg = i18n("You can use this to add additional info to a "
                                     "certification.") + QStringLiteral("<br/><br/>") +
                                     i18n("Tags created by anyone with full certification trust "
                                          "are shown in the keylist and can be searched.");
            QToolTip::showText(infoBtn->mapToGlobal(QPoint()) + QPoint(infoBtn->width(), 0),
                               msg, infoBtn, QRect(), 30000);
        });

        auto tagsLay = new QHBoxLayout;
        tagsLay->addWidget(infoBtn);
        tagsLay->addWidget(mTagsLE);

        advLay->addWidget(mExportCB);
        advLay->addLayout(publishLay);
        advLay->addLayout(tagsLay);

#ifndef GPGME_HAS_REMARKS
        // Hide it if we do not have remark support
        mTagsLE->setVisible(false);
        infoBtn->setVisible(false);
#endif

        expander->setContentLayout(advLay);

        mPublishCB->setEnabled(false);

        connect(mExportCB, &QCheckBox::toggled, [this] (bool on) {
            mPublishCB->setEnabled(on);
        });

        connect(mSecKeySelect, &KeySelectionCombo::currentKeyChanged, [this] (const GpgME::Key &) {
#ifdef GPGME_HAS_REMARKS
            updateTags();
#endif
        });

        loadConfig();
    }

    ~Private()
    {
    }

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

    void setTarget(const GpgME::Key &key)
    {
        mFprLabel->setText(i18n("Fingerprint: <b>%1</b>",
                            Formatting::prettyID(key.primaryFingerprint())) + QStringLiteral("<br/>") +
                            i18n("<i>Only the fingerprint clearly identifies the key and its owner.</i>"));
        mUserIDModel.setKey(key);
        mTarget = key;

        updateTags();
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

private:
    CertifyWidget *const q;
    QLabel *mFprLabel;
    KeySelectionCombo *mSecKeySelect;
    QCheckBox *mExportCB;
    QCheckBox *mPublishCB;
    QLineEdit *mTagsLE;

    UserIDModel mUserIDModel;
    GpgME::Key mTarget;
};

CertifyWidget::CertifyWidget(QWidget *parent) :
    QWidget(parent),
    d(new Private(this))
{
}

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

// For UserID model
#include "certifywidget.moc"
