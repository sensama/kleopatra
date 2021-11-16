/*
    conf/groupsconfigdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "groupsconfigdialog.h"

#include "groupsconfigpage.h"

#include <KConfigGroup>
#include <KGuiItem>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KStandardGuiItem>

#include <QDialogButtonBox>
#include <QPushButton>

class GroupsConfigDialog::Private
{
    friend class ::GroupsConfigDialog;
    GroupsConfigDialog *const q;

    GroupsConfigPage *configPage = nullptr;

public:
    Private(GroupsConfigDialog *qq)
        : q(qq)
        , configPage(new GroupsConfigPage(qq))
    {
        restoreLayout();
    }

    ~Private()
    {
        saveLayout();
    }

private:
    void saveLayout()
    {
        KConfigGroup configGroup(KSharedConfig::openStateConfig(), "GroupsConfigDialog");
        configGroup.writeEntry("Size", q->size());
        configGroup.sync();
    }

    void restoreLayout(const QSize &defaultSize = QSize())
    {
        const KConfigGroup configGroup(KSharedConfig::openStateConfig(), "GroupsConfigDialog");
        const QSize size = configGroup.readEntry("Size", defaultSize);
        if (size.isValid()) {
            q->resize(size);
        }
    }
};

GroupsConfigDialog::GroupsConfigDialog(QWidget *parent)
    : KConfigDialog(parent, GroupsConfigDialog::dialogName(), /*config=*/ nullptr)
    , d(new Private(this))
{
    setWindowTitle(i18nc("@title:window", "Configure Groups"));
    setFaceType(KPageDialog::Plain);

    addPage(d->configPage, i18n("Groups"), /*pixmapName=*/ QString(), /*header=*/ QString(), /*manage=*/ false);

    // there are no defaults to restore
    buttonBox()->removeButton(buttonBox()->button(QDialogButtonBox::RestoreDefaults));

    QPushButton *resetButton = buttonBox()->addButton(QDialogButtonBox::Reset);
    KGuiItem::assign(resetButton, KStandardGuiItem::reset());
    resetButton->setEnabled(false);

    connect(buttonBox()->button(QDialogButtonBox::Reset), &QAbstractButton::clicked,
            this, &GroupsConfigDialog::updateWidgets);

    connect(d->configPage, &GroupsConfigPage::changed, this, [this] (bool state) {
        updateButtons();
        if (QPushButton *button = buttonBox()->button(QDialogButtonBox::Reset)) {
            button->setEnabled(d->configPage->hasChanged());
        }
    });
}

GroupsConfigDialog::~GroupsConfigDialog() = default;

QString GroupsConfigDialog::dialogName()
{
    return QStringLiteral("Group Settings");
}

void GroupsConfigDialog::updateSettings()
{
    d->configPage->save();
}

void GroupsConfigDialog::updateWidgets()
{
    d->configPage->load();
}

bool GroupsConfigDialog::hasChanged()
{
    return d->configPage->hasChanged();
}
