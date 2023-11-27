/*
    conf/groupsconfigdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "groupsconfigdialog.h"

#include "groupsconfigpage.h"

#include "utils/gui-helper.h"

#include <KConfigGroup>
#include <KGuiItem>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KStandardGuiItem>

#include <Libkleo/DocAction>

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>

class GroupsConfigDialog::Private
{
    friend class ::GroupsConfigDialog;
    GroupsConfigDialog *const q;

    GroupsConfigPage *configPage = nullptr;
    bool initialFocusWasSet = false;

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

    void setInitialFocus()
    {
        if (initialFocusWasSet) {
            return;
        }
        // this is a bit hacky, but fixing the focus chain where the dialog
        // button box comes before the page, which causes the first button in
        // the button box to be focussed initially, is even more hacky
        Q_ASSERT(configPage->findChildren<QLineEdit *>().size() == 1);
        if (auto searchField = configPage->findChild<QLineEdit *>()) {
            searchField->setFocus();
        }
        initialFocusWasSet = true;
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
    : KConfigDialog(parent, GroupsConfigDialog::dialogName(), /*config=*/nullptr)
    , d(new Private(this))
{
    setWindowTitle(i18nc("@title:window", "Configure Groups"));
    setFaceType(KPageDialog::Plain);

    const auto *const item = addPage(d->configPage, i18n("Groups"), /*pixmapName=*/QString(), /*header=*/QString(), /*manage=*/false);
    // prevent scroll area embedding the config page from receiving focus
    const auto scrollAreas = item->widget()->findChildren<QScrollArea *>();
    for (auto sa : scrollAreas) {
        sa->setFocusPolicy(Qt::NoFocus);
    }

    // there are no defaults to restore
    buttonBox()->removeButton(buttonBox()->button(QDialogButtonBox::RestoreDefaults));

    QPushButton *resetButton = buttonBox()->addButton(QDialogButtonBox::Reset);
    KGuiItem::assign(resetButton, KStandardGuiItem::reset());
    resetButton->setEnabled(false);

    const auto helpAction =
        new Kleo::DocAction(QIcon::fromTheme(QStringLiteral("help")),
                            i18n("Help"),
                            i18nc("Only available in German and English. Leave to English for other languages.", "handout_group-feature_gnupg_en.pdf"),
                            QStringLiteral("../share/doc/gnupg-vsd"));
    if (helpAction->isEnabled()) {
        auto helpButton = buttonBox()->button(QDialogButtonBox::Help);
        if (helpButton) {
            disconnect(helpButton, &QAbstractButton::clicked, nullptr, nullptr);
            connect(helpButton, &QAbstractButton::clicked, helpAction, &QAction::trigger);
            connect(helpButton, &QObject::destroyed, helpAction, &QObject::deleteLater);
        }
    } else {
        delete helpAction;
    }

    // prevent accidental closing of dialog when pressing Enter while the search field has focus
    Kleo::unsetAutoDefaultButtons(this);

    connect(buttonBox()->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, this, &GroupsConfigDialog::updateWidgets);

    connect(d->configPage, &GroupsConfigPage::changed, this, [this]() {
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

void GroupsConfigDialog::showEvent(QShowEvent *event)
{
    d->setInitialFocus();

    KConfigDialog::showEvent(event);

    // prevent accidental closing of dialog when pressing Enter while the search field has focus
    Kleo::unsetDefaultButtons(buttonBox());
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
