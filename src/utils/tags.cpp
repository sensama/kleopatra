/*  utils/tags.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019 g10code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tags.h"

#include "tagspreferences.h"

#include "kleopatra_debug.h"

#include <KSharedConfig>
#include <KConfigGroup>
#include <Libkleo/KeyCache>

using namespace Kleo;

bool Tags::tagsEnabled()
{
    return TagsPreferences().useTags();
}

void Tags::enableTags()
{
    TagsPreferences().setUseTags(true);
    KeyCache::mutableInstance()->enableRemarks(true);
}

GpgME::Key Tags::tagKey()
{
    const auto tagKeyFpr = TagsPreferences().tagKey();
    GpgME::Key key;
    if (tagKeyFpr.isEmpty()) {
        return key;
    }
    key = KeyCache::instance()->findByKeyIDOrFingerprint(tagKeyFpr.toLatin1().constData());
    if (key.isNull()) {
        qCDebug(KLEOPATRA_LOG) << "Failed to find tag key: " << tagKeyFpr;
        return key;
    }
    return key;
}

std::vector<GpgME::Key> Tags::tagKeys()
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

void Tags::setTagKey(const GpgME::Key &key)
{
    TagsPreferences().setTagKey(key.isNull() ? QString() : QString::fromLatin1(key.primaryFingerprint()));
}
