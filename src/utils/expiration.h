/* -*- mode: c++; c-basic-offset:4 -*-
    utils/expiration.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2023 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDate>

class KDateComboBox;

namespace Kleo
{
    struct DateRange {
        QDate minimum;
        QDate maximum;
    };

    /**
     * Returns the earliest allowed expiration date.
     *
     * This is either tomorrow or the configured number of days after today
     * (whichever is later).
     *
     * \sa Settings::validityPeriodInDaysMin
     */
    QDate minimumExpirationDate();

    /**
     * Returns the latest allowed expiration date.
     *
     * If unlimited validity is allowed, then an invalid date is returned.
     * Otherwise, the configured number of days after today is returned.
     * Additionally, the returned date is never earlier than the minimum
     * expiration date.
     *
     * \sa Settings::validityPeriodInDaysMax
     */
    QDate maximumExpirationDate();

    /**
     * Returns the allowed range for the expiration date.
     *
     * \sa minimumExpirationDate, maximumExpirationDate
     */
    DateRange expirationDateRange();

    /**
     * Configures the date combo box \p dateCB for choosing an expiration date.
     *
     * Sets the allowed date range and a tooltip. And disables the combo box
     * if a fixed validity period is configured.
     */
    void setUpExpirationDateComboBox(KDateComboBox *dateCB);
}
