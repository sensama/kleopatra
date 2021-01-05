#ifndef REMARKS_H
#define REMARKS_H
/*  utils/remarks.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2019 g 10code GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <gpgme++/key.h>

#include <vector>

namespace Kleo
{
namespace Remarks
{
/* Helper functions to work with remark configuration */
bool remarksEnabled();
void enableRemarks();
/* Read / write a single remark key into configuration. */
GpgME::Key remarkKey();
void setRemarkKey(const GpgME::Key &key);

/* Get multiple keys to use for remarks. Currently
 * this returns all fully trusted OpenPGP Keys. */
std::vector<GpgME::Key> remarkKeys();
}
} // namespace Kleo
#endif // REMARKS_H
