/*  utils/remarks.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019 g 10code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "remarks.h"

#include "tagspreferences.h"

#include "kleopatra_debug.h"

#include <KSharedConfig>
#include <KConfigGroup>
#include <Libkleo/KeyCache>

using namespace Kleo;

bool Remarks::remarksEnabled()
{
    return TagsPreferences().useTags();
}

void Remarks::enableRemarks()
{
    TagsPreferences().setUseTags(true);
    KeyCache::mutableInstance()->enableRemarks(true);
}

GpgME::Key Remarks::remarkKey()
{
    const auto remarkKeyFpr = TagsPreferences().tagKey();
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
    TagsPreferences().setTagKey(key.isNull() ? QString() : QString::fromLatin1(key.primaryFingerprint()));
}
