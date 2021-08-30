/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/expirydialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "expirydialog.h"

#include <KLocalizedString>
#include <KStandardGuiItem>

#include <QCalendarWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace Kleo;
using namespace Kleo::Dialogs;

namespace
{

enum Period {
    Days,
    Weeks,
    Months,
    Years,

    NumPeriods
};

static QDate date_by_amount_and_unit(int inAmount, int inUnit)
{
    const QDate current = QDate::currentDate();
    switch (inUnit) {
    case Days:   return current.addDays(inAmount);
    case Weeks:  return current.addDays(7 * inAmount);
    case Months: return current.addMonths(inAmount);
    case Years:  return current.addYears(inAmount);
    default:
        Q_ASSERT(!"Should not reach here");
    }
    return QDate();
}

// these calculations should be precise enough for the forseeable future...
static const double DAYS_IN_GREGORIAN_YEAR = 365.2425;

static int monthsBetween(const QDate &d1, const QDate &d2)
{
    const int days = d1.daysTo(d2);
    return qRound(days / DAYS_IN_GREGORIAN_YEAR * 12);
}

static int yearsBetween(const QDate &d1, const QDate &d2)
{
    const int days = d1.daysTo(d2);
    return qRound(days / DAYS_IN_GREGORIAN_YEAR);
}

auto radioButtonSize(const QRadioButton *radioButton)
{
    QStyleOptionButton opt;
    return radioButton->style()->sizeFromContents(QStyle::CT_RadioButton, &opt, QSize(), radioButton);
}

}

class ExpiryDialog::Private
{
    friend class ::Kleo::Dialogs::ExpiryDialog;
    ExpiryDialog *const q;
public:
    explicit Private(Mode mode, ExpiryDialog *qq)
        : q{qq}
        , mode{mode}
        , inUnit{Days}
        , ui{mode, q}
    {
        connect(ui.inSB, &QSpinBox::valueChanged,
                q, [this] () { slotInAmountChanged(); });
        connect(ui.inCB, qOverload<int>(&QComboBox::currentIndexChanged),
                q, [this] () { slotInUnitChanged(); });
        connect(ui.onCW, &QCalendarWidget::selectionChanged,
                q, [this] () { slotOnDateChanged(); });
        connect(ui.onCW, &QCalendarWidget::currentPageChanged,
                q, [this] (int year, int month) {
                    // We select the same day in the month when
                    // a page is switched.
                    auto date = ui.onCW->selectedDate();
                    if (!date.setDate(year, month, date.day())) {
                        date.setDate(year, month, 1);
                    }
                    ui.onCW->setSelectedDate(date);
                });

        Q_ASSERT(ui.inCB->currentIndex() == inUnit);
    }

private:
    void slotInAmountChanged();
    void slotInUnitChanged();
    void slotOnDateChanged();

private:
    QDate inDate() const;
    int inAmountByDate(const QDate &date) const;

private:
    ExpiryDialog::Mode mode;
    int inUnit;

    struct UI {
        QRadioButton *neverRB;
        QRadioButton *inRB;
        QSpinBox *inSB;
        QComboBox *inCB;
        QRadioButton *onRB;
        QCalendarWidget *onCW;
        QCheckBox *updateSubkeysCheckBox;

        explicit UI(Mode mode, Dialogs::ExpiryDialog *qq)
        {
            auto mainLayout = new QVBoxLayout{qq};

            auto mainWidget = new QWidget{qq};

            auto vboxLayout = new QVBoxLayout{mainWidget};
            vboxLayout->setContentsMargins(0, 0, 0, 0);

            {
                auto label = new QLabel{qq};
                label->setText(mode == Mode::UpdateIndividualSubkey ?
                               i18n("Please select when to expire this subkey:") :
                               i18n("Please select when to expire this certificate:"));
                vboxLayout->addWidget(label);
            }

            neverRB = new QRadioButton(i18n("Ne&ver"), mainWidget);
            neverRB->setChecked(false);

            vboxLayout->addWidget(neverRB);

            {
                auto hboxLayout = new QHBoxLayout;

                inRB = new QRadioButton{i18n("In"), mainWidget};
                inRB->setChecked(false);

                hboxLayout->addWidget(inRB);

                inSB = new QSpinBox{mainWidget};
                inSB->setEnabled(false);
                inSB->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
                inSB->setMinimum(1);

                hboxLayout->addWidget(inSB);

                inCB = new QComboBox{mainWidget};
                inCB->addItem(i18n("Days"));
                inCB->addItem(i18n("Weeks"));
                inCB->addItem(i18n("Months"));
                inCB->addItem(i18n("Years"));
                Q_ASSERT(inCB->count() == NumPeriods);
                inCB->setEnabled(false);

                hboxLayout->addWidget(inCB);

                hboxLayout->addStretch(1);

                vboxLayout->addLayout(hboxLayout);
            }

            onRB = new QRadioButton{i18n("On this da&y:"), mainWidget};
            onRB->setChecked(true);

            vboxLayout->addWidget(onRB);

            {
                auto hboxLayout = new QHBoxLayout;

                hboxLayout->addSpacing(radioButtonSize(onRB).width());

                onCW = new QCalendarWidget{mainWidget};
                onCW->setGridVisible(true);
                onCW->setMinimumDate(QDate::currentDate().addDays(1));

                hboxLayout->addWidget(onCW);

                hboxLayout->addStretch(1);

                vboxLayout->addLayout(hboxLayout);
            }

            {
                updateSubkeysCheckBox = new QCheckBox{i18n("Also update the expiry of the subkeys"), qq};
#ifdef QGPGME_SUPPORTS_CHANGING_EXPIRATION_OF_COMPLETE_KEY
                updateSubkeysCheckBox->setVisible(mode == Mode::UpdateCertificateWithSubkeys);
#else
                updateSubkeysCheckBox->setVisible(false);
#endif
                vboxLayout->addWidget(updateSubkeysCheckBox);
            }

            vboxLayout->addStretch(1);

            mainLayout->addWidget(mainWidget);

            auto buttonBox = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, qq};
            auto okButton = buttonBox->button(QDialogButtonBox::Ok);
            KGuiItem::assign(okButton, KStandardGuiItem::ok());
            okButton->setDefault(true);
            okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
            KGuiItem::assign(buttonBox->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());
            qq->connect(buttonBox, &QDialogButtonBox::accepted, qq, &QDialog::accept);
            qq->connect(buttonBox, &QDialogButtonBox::rejected, qq, &QDialog::reject);

            mainLayout->addWidget(buttonBox);

            connect(onRB, &QRadioButton::toggled, onCW, &QWidget::setEnabled);
            connect(inRB, &QRadioButton::toggled, inCB, &QWidget::setEnabled);
            connect(inRB, &QRadioButton::toggled, inSB, &QWidget::setEnabled);
        }
    } ui;
};

void ExpiryDialog::Private::slotInUnitChanged()
{
    const int oldInAmount = ui.inSB->value();
    const QDate targetDate = date_by_amount_and_unit(oldInAmount, inUnit);
    inUnit = ui.inCB->currentIndex();
    if (targetDate.isValid()) {
        ui.inSB->setValue(inAmountByDate(targetDate));
    } else {
        slotInAmountChanged();
    }
}

void ExpiryDialog::Private::slotInAmountChanged()
{
    // Only modify onCW when onCW is slave:
    if (ui.inRB->isChecked()) {
        ui.onCW->setSelectedDate(inDate());
    }
}

void ExpiryDialog::Private::slotOnDateChanged()
{
    // Only modify inSB/inCB when onCW is master:
    if (ui.onRB->isChecked()) {
        ui.inSB->setValue(inAmountByDate(ui.onCW->selectedDate()));
    }
}

QDate ExpiryDialog::Private::inDate() const
{
    return date_by_amount_and_unit(ui.inSB->value(), ui.inCB->currentIndex());
}

int ExpiryDialog::Private::inAmountByDate(const QDate &selected) const
{
    const QDate current  = QDate::currentDate();

    switch (ui.inCB->currentIndex()) {
    case Days:   return current.daysTo(selected);
    case Weeks:  return qRound(current.daysTo(selected) / 7.0);
    case Months: return monthsBetween(current, selected);
    case Years:  return yearsBetween(current, selected);
    };
    Q_ASSERT(!"Should not reach here");
    return -1;
}

ExpiryDialog::ExpiryDialog(Mode mode, QWidget *p)
    : QDialog{p}
    , d{new Private{mode, this}}
{
    setWindowTitle(i18nc("@title:window", "Change Expiry"));
}

ExpiryDialog::~ExpiryDialog() = default;

void ExpiryDialog::setDateOfExpiry(const QDate &date)
{
    const QDate current = QDate::currentDate();
    if (date.isValid()) {
        d->ui.onRB->setChecked(true);
        d->ui.onCW->setSelectedDate(qMax(date, current));
    } else {
        d->ui.neverRB->setChecked(true);
        d->ui.onCW->setSelectedDate(current);
        d->ui.inSB->setValue(0);
    }
}

QDate ExpiryDialog::dateOfExpiry() const
{
    return
        d->ui.inRB->isChecked() ? d->inDate() :
        d->ui.onRB->isChecked() ? d->ui.onCW->selectedDate() :
        QDate{};
}

void ExpiryDialog::setUpdateExpirationOfAllSubkeys(bool update)
{
    d->ui.updateSubkeysCheckBox->setChecked(update);
}

bool ExpiryDialog::updateExpirationOfAllSubkeys() const
{
    return d->ui.updateSubkeysCheckBox->isChecked();
}

#include "moc_expirydialog.cpp"
