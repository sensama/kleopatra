/*  utils/tags.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019 g10code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <gpgme++/key.h>

#include <vector>

namespace Kleo
{
namespace Tags
{
/* Helper functions to work with tags configuration */
bool tagsEnabled();
void enableTags();
/* Read / write a single tag key into configuration. */
GpgME::Key tagKey();
void setTagKey(const GpgME::Key &key);

/* Get multiple keys to use for tags. Currently
 * this returns all fully trusted OpenPGP Keys. */
std::vector<GpgME::Key> tagKeys();
}
} // namespace Kleo
