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

private:
    GroupsConfigWidget *widget = nullptr;
};

GroupsConfigPage::Private::Private(GroupsConfigPage *qq)
    : q(qq)
{
}

GroupsConfigPage::GroupsConfigPage(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , d(new Private(this))
{
    // do not show the Defaults button; it has no effect
    setButtons(buttons() & ~KCModule::Default);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    d->widget = new Kleo::GroupsConfigWidget(this);
    if (QLayout *l = d->widget->layout()) {
        l->setContentsMargins(0, 0, 0, 0);
    }

    layout->addWidget(d->widget);

    connect(d->widget, &GroupsConfigWidget::changed, this, &GroupsConfigPage::markAsChanged);
}

GroupsConfigPage::~GroupsConfigPage() = default;

void GroupsConfigPage::load()
{
    qCDebug(KLEOPATRA_LOG) << "GroupsConfigPage::load()";
    d->widget->setGroups(KeyCache::instance()->configurableGroups());
}

void GroupsConfigPage::save()
{
    KeyCache::mutableInstance()->saveConfigurableGroups(d->widget->groups());

    // reload after saving to ensure that the groups reflect the saved groups (e.g. in case of immutable entries)
    load();
}

extern "C"
{
    Q_DECL_EXPORT KCModule *create_kleopatra_config_groups(QWidget *parent = nullptr, const QVariantList &args = QVariantList())
    {
        GroupsConfigPage *page = new GroupsConfigPage(parent, args);
        page->setObjectName(QStringLiteral("kleopatra_config_groups"));
        return page;
    }
}
