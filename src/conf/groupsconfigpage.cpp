/*
    conf/groupsconfigpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "groupsconfigpage.h"

#include "groupsconfigwidget.h"

#include <Libkleo/Debug>
#include <Libkleo/KeyCache>
#include <Libkleo/KeyGroup>

#include <QVBoxLayout>

#include "kleopatra_debug.h"

using namespace Kleo;

class GroupsConfigPage::Private
{
    friend class ::GroupsConfigPage;
    GroupsConfigPage *const q;

    Private(GroupsConfigPage *qq);
public:
    ~Private() = default;

    void setChanged(bool changed);

private:
    GroupsConfigWidget *widget = nullptr;
    bool changed = false;
};

GroupsConfigPage::Private::Private(GroupsConfigPage *qq)
    : q(qq)
{
}

void GroupsConfigPage::Private::setChanged(bool state)
{
    changed = state;
    Q_EMIT q->changed(changed);
}

GroupsConfigPage::GroupsConfigPage(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    d->widget = new Kleo::GroupsConfigWidget(this);
    if (QLayout *l = d->widget->layout()) {
        l->setContentsMargins(0, 0, 0, 0);
    }

    layout->addWidget(d->widget);

    connect(d->widget, &GroupsConfigWidget::changed, this, [this] () { d->setChanged(true); });
}

GroupsConfigPage::~GroupsConfigPage() = default;

bool GroupsConfigPage::hasChanged() const
{
    return d->changed;
}

void GroupsConfigPage::load()
{
    d->widget->setGroups(KeyCache::instance()->configurableGroups());
    d->setChanged(false);
}

void GroupsConfigPage::save()
{
    KeyCache::mutableInstance()->saveConfigurableGroups(d->widget->groups());

    // reload after saving to ensure that the groups reflect the saved groups (e.g. in case of immutable entries)
    load();
}
