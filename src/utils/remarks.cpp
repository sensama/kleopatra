/*  utils/remarks.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019 g 10code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "remarks.h"
#include "kleopatra_debug.h"

#include <KSharedConfig>
#include <KConfigGroup>
#include <Libkleo/KeyCache>

using namespace Kleo;

bool Remarks::remarksEnabled()
{
    const KConfigGroup conf(KSharedConfig::openConfig(), "RemarkSettings");
    return conf.readEntry("RemarksEnabled", false);
}

void Remarks::enableRemarks(bool enable)
{
    KConfigGroup conf(KSharedConfig::openConfig(), "RemarkSettings");
    conf.writeEntry("RemarksEnabled", enable);
    KeyCache::mutableInstance()->enableRemarks(enable);
}

GpgME::Key Remarks::remarkKey()
{
    const KConfigGroup conf(KSharedConfig::openConfig(), "RemarkSettings");
    const auto remarkKeyFpr = conf.readEntry("RemarkKeyFpr", QString());
    GpgME::Key key;
    if (remarkKeyFpr.isEmpty()) {
        return key;
    }
    key = KeyCache::instance()->findByKeyIDOrFingerprint(remarkKeyFpr.toLatin1().constData());
    if (key.isNull()) {
        qCDebug(KLEOPATRA_LOG) << "Failed to find remark key: " << remarkKeyFpr;
        return key;
    }
    return key;
}

std::vector<GpgME::Key> Remarks::remarkKeys()
{
    std::vector<GpgME::Key> ret;
    for (const auto &key: KeyCache::instance()->keys()) {
        if (key.isNull() || key.isRevoked() || key.isExpired() ||
            key.isDisabled() || key.isInvalid() || key.protocol() != GpgME::OpenPGP) {
            continue;
        }
        if (key.ownerTrust() >= GpgME::Key::Full) {
            ret.push_back(key);
        }
    }
    return ret;
}

void Remarks::setRemarkKey(const GpgME::Key &key)
{
    KConfigGroup conf(KSharedConfig::openConfig(), "RemarkSettings");
    conf.writeEntry("RemarkKeyFpr", key.isNull() ? QString() : QString::fromLatin1(key.primaryFingerprint()));
}
