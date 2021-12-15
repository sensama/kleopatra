/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/revokecertificationwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "revokecertificationwidget.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <Libkleo/DefaultKeyFilter>
#include <Libkleo/Formatting>
#include <Libkleo/KeySelectionCombo>

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <gpgme++/key.h>

#include "kleopatra_debug.h"

using namespace Kleo;

namespace {

class CertificationKeyFilter: public DefaultKeyFilter
{
public:
    CertificationKeyFilter(const GpgME::Key &certificationTarget);

    bool matches(const GpgME::Key &key, Kleo::KeyFilter::MatchContexts contexts) const override;

private:
    GpgME::Key mCertificationTarget; // the key to certify or to revoke the certification of
};

CertificationKeyFilter::CertificationKeyFilter(const GpgME::Key &certificationTarget)
    : DefaultKeyFilter()
    , mCertificationTarget(certificationTarget)
{
    setIsOpenPGP(DefaultKeyFilter::Set);
    setHasSecret(DefaultKeyFilter::Set);
    setCanCertify(DefaultKeyFilter::Set);
    setIsBad(DefaultKeyFilter::NotSet);
}

bool CertificationKeyFilter::matches(const GpgME::Key &key, Kleo::KeyFilter::MatchContexts contexts) const
{
    if (!(availableMatchContexts() & contexts)) {
        return false;
    }
    // exclude certification target from list of certification keys
    if (qstrcmp(key.primaryFingerprint(), mCertificationTarget.primaryFingerprint()) == 0) {
        return false;
    }
    return DefaultKeyFilter::matches(key, contexts);
}

static bool uidsAreEqual(const GpgME::UserID &lhs, const GpgME::UserID &rhs)
{
    // use uidhash if available
    if (lhs.uidhash() && rhs.uidhash()) {
        return strcmp(lhs.uidhash(), rhs.uidhash()) == 0;
    }
    // compare actual user ID string and primary key; this is not unique, but it's all we can do if uidhash is missing
    return qstrcmp(lhs.id(), rhs.id()) == 0
        && qstrcmp(lhs.parent().primaryFingerprint(), rhs.parent().primaryFingerprint()) == 0;
}

class UserIDModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit UserIDModel(QObject *parent = nullptr) : QStandardItemModel(parent)
    {
    }

    void setKey(const GpgME::Key &key)
    {
        mKey = key;
        clear();
        const std::vector<GpgME::UserID> uids = key.userIDs();
        for (const auto &uid : uids) {
            auto const item = new QStandardItem;
            item->setText(Formatting::prettyUserID(uid));
            item->setCheckable(true);
            item->setEditable(false);
            item->setCheckState(Qt::Checked);
            appendRow(item);
        }
    }

    void setCheckedUserIDs(const std::vector<GpgME::UserID> &checkedUids)
    {
        const auto keyUids = mKey.userIDs();
        Q_ASSERT(rowCount() == static_cast<int>(keyUids.size()));

        for (int i = 0; i < rowCount(); ++i) {
            const auto &keyUid = keyUids[i];
            const bool uidIsChecked = std::find_if(checkedUids.cbegin(), checkedUids.cend(),
                                                   [keyUid](const GpgME::UserID &checkedUid) { return uidsAreEqual(keyUid, checkedUid); }) != checkedUids.cend();
            item(i)->setCheckState(uidIsChecked ? Qt::Checked : Qt::Unchecked);
        }
    }

    std::vector<GpgME::UserID> checkedUserIDs() const
    {
        const auto keyUids = mKey.userIDs();
        Q_ASSERT(rowCount() == static_cast<int>(keyUids.size()));

        std::vector<GpgME::UserID> checkedUids;
        for (int i = 0; i < rowCount(); ++i) {
            if (item(i)->checkState() == Qt::Checked) {
                checkedUids.push_back(keyUids[i]);
            }
        }
        return checkedUids;
    }

private:
    GpgME::Key mKey;
};

} // unnamed namespace

class RevokeCertificationWidget::Private
{
    friend class ::Kleo::RevokeCertificationWidget;
    RevokeCertificationWidget *const q;

    QLabel *mFprLabel;
    KeySelectionCombo *mCertificationKeySelect;
    QCheckBox *mPublishCB;

    UserIDModel mUserIDModel;
    GpgME::Key mTarget;

public:
    Private(RevokeCertificationWidget *qq)
        : q(qq)
        , mFprLabel(new QLabel)
        , mCertificationKeySelect(new KeySelectionCombo(/* secretOnly = */ true))
        , mPublishCB(new QCheckBox)
    {
        auto mainLayout = new QVBoxLayout(q);
        mainLayout->addWidget(mFprLabel);

        auto certKeyLayout = new QHBoxLayout;
        {
            auto label = new QLabel(i18n("Certification key:"));
            label->setToolTip(i18n("The key whose certifications shall be revoke"));
            certKeyLayout->addWidget(label);
        }
        certKeyLayout->addWidget(mCertificationKeySelect, 1);
        mainLayout->addLayout(certKeyLayout);

        auto splitLine = new QFrame;
        splitLine->setFrameShape(QFrame::HLine);
        splitLine->setFrameShadow(QFrame::Sunken);
        splitLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

        mainLayout->addWidget(splitLine);

        auto listView = new QListView;
        listView->setModel(&mUserIDModel);
        mainLayout->addWidget(listView, 1);

        mPublishCB = new QCheckBox(i18n("Publish revocations on keyserver"));
        mainLayout->addWidget(mPublishCB);

        loadConfig();
    }

    ~Private()
    {
    }

    void saveConfig()
    {
        KConfigGroup conf(KSharedConfig::openConfig(), "RevokeCertificationSettings");
        const auto certificationKey = mCertificationKeySelect->currentKey();
        if (!certificationKey.isNull()) {
            conf.writeEntry("LastKey", certificationKey.primaryFingerprint());
        }
        conf.writeEntry("PublishCheckState", mPublishCB->isChecked());
    }

    void loadConfig()
    {
        const KConfigGroup conf(KSharedConfig::openConfig(), "RevokeCertificationSettings");
        mCertificationKeySelect->setDefaultKey(conf.readEntry("LastKey", QString()));
        mPublishCB->setChecked(conf.readEntry("PublishCheckState", false));
    }
};

RevokeCertificationWidget::RevokeCertificationWidget(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{
}

RevokeCertificationWidget::~RevokeCertificationWidget()
{
}

void RevokeCertificationWidget::setTarget(const GpgME::Key &key)
{
    d->mTarget = key;
    d->mFprLabel->setText(i18n("Fingerprint: <b>%1</b>",
                          Formatting::prettyID(d->mTarget.primaryFingerprint())) + QStringLiteral("<br/>") +
                          i18n("<i>Only the fingerprint clearly identifies the key and its owner.</i>"));
    d->mCertificationKeySelect->setKeyFilter(std::shared_ptr<KeyFilter>(new CertificationKeyFilter(d->mTarget)));
    d->mUserIDModel.setKey(d->mTarget);
}

GpgME::Key RevokeCertificationWidget::target() const
{
    return d->mTarget;
}

void RevokeCertificationWidget::setSelectUserIDs(const std::vector<GpgME::UserID> &uids)
{
    d->mUserIDModel.setCheckedUserIDs(uids);
}

std::vector<GpgME::UserID> RevokeCertificationWidget::selectedUserIDs() const
{
    return d->mUserIDModel.checkedUserIDs();
}

void RevokeCertificationWidget::setCertificationKey(const GpgME::Key &key)
{
    d->mCertificationKeySelect->setDefaultKey(QString::fromLatin1(key.primaryFingerprint()));
}

GpgME::Key RevokeCertificationWidget::certificationKey() const
{
    return d->mCertificationKeySelect->currentKey();
}

bool RevokeCertificationWidget::publishSelected() const
{
    return d->mPublishCB->isChecked();
}

void Kleo::RevokeCertificationWidget::saveConfig() const
{
    d->saveConfig();
}

// for UserIDModel
#include "revokecertificationwidget.moc"
