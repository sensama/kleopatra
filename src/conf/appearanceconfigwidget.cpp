/*
    appearanceconfigwidget.cpp

    This file is part of kleopatra, the KDE key manager
    SPDX-FileCopyrightText: 2002, 2004, 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2002, 2003 Marc Mutz <mutz@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config-kleopatra.h>

#include "appearanceconfigwidget.h"
#include "ui_appearanceconfigwidget.h"

#include "tagspreferences.h"
#include "tooltippreferences.h"

#include <settings.h>

#include <Libkleo/KeyFilterManager>
#include <Libkleo/Dn>
#include <Libkleo/DNAttributeOrderConfigWidget>
#include <Libkleo/SystemInfo>

#include <KIconDialog>
#include <QIcon>

#include <KConfig>
#include <KLocalizedString>
#include <KConfigGroup>

#include <QColor>
#include <QFont>
#include <QString>
#include <QRegularExpression>
#include <QApplication>
#include <QColorDialog>
#include <QFontDialog>

#include <algorithm>

using namespace Kleo;
using namespace Kleo::Config;

enum {
    HasNameRole = Qt::UserRole + 0x1234, /*!< Records that the user has assigned a name (to avoid comparing with i18n-strings) */
    HasFontRole,                         /*!< Records that the user has chosen  completely different font (as opposed to italic/bold/strikeout) */
    IconNameRole,                        /*!< Records the name of the icon (since QIcon won't give it out again, once set) */
    MayChangeNameRole,
    MayChangeForegroundRole,
    MayChangeBackgroundRole,
    MayChangeFontRole,
    MayChangeItalicRole,
    MayChangeBoldRole,
    MayChangeStrikeOutRole,
    MayChangeIconRole,
    StoredForegroundRole, /*!< Stores the actual configured foreground color */
    StoredBackgroundRole, /*!< Stores the actual configured background color */

    EndDummy,
};

static QFont tryToFindFontFor(const QListWidgetItem *item)
{
    if (item)
        if (const QListWidget *const lw = item->listWidget()) {
            return lw->font();
        }
    return QApplication::font("QListWidget");
}

static bool is(const QListWidgetItem *item, bool (QFont::*func)() const)
{
    if (!item) {
        return false;
    }
    const QVariant v = item->data(Qt::FontRole);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (!v.isValid() || v.type() != QVariant::Font) {
#else
    if (!v.isValid() || v.userType() != QMetaType::QFont) {
#endif
        return false;
    }
    return (v.value<QFont>().*func)();
}

static bool is_italic(const QListWidgetItem *item)
{
    return is(item, &QFont::italic);
}
static bool is_bold(const QListWidgetItem *item)
{
    return is(item, &QFont::bold);
}
static bool is_strikeout(const QListWidgetItem *item)
{
    return is(item, &QFont::strikeOut);
}

static void set(QListWidgetItem *item, bool on, void (QFont::*func)(bool))
{
    if (!item) {
        return;
    }
    const QVariant v = item->data(Qt::FontRole);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QFont font = v.isValid() && v.type() == QVariant::Font ? v.value<QFont>() : tryToFindFontFor(item);
#else
    QFont font = v.isValid() && v.userType() == QMetaType::QFont ? v.value<QFont>() : tryToFindFontFor(item);
#endif
    (font.*func)(on);
    item->setData(Qt::FontRole, font);
}

static void set_italic(QListWidgetItem *item, bool on)
{
    set(item, on, &QFont::setItalic);
}
static void set_bold(QListWidgetItem *item, bool on)
{
    set(item, on, &QFont::setBold);
}
static void set_strikeout(QListWidgetItem *item, bool on)
{
    set(item, on, &QFont::setStrikeOut);
}

static void apply_config(const KConfigGroup &group, QListWidgetItem *item)
{
    if (!item) {
        return;
    }

    const QString name = group.readEntry("Name");
    item->setText(name.isEmpty() ? i18nc("Key filter without user-assigned name", "<unnamed>") : name);
    item->setData(HasNameRole, !name.isEmpty());
    item->setData(MayChangeNameRole, !group.isEntryImmutable("Name"));

    const QColor fg = group.readEntry("foreground-color", QColor());
    item->setData(StoredForegroundRole, fg.isValid() ? QBrush(fg) : QVariant());
    if (!SystemInfo::isHighContrastModeActive()) {
        item->setData(Qt::ForegroundRole, fg.isValid() ? QBrush(fg) : QVariant());
    }
    item->setData(MayChangeForegroundRole, !group.isEntryImmutable("foreground-color"));

    const QColor bg = group.readEntry("background-color", QColor());
    item->setData(StoredBackgroundRole, bg.isValid() ? QBrush(bg) : QVariant());
    if (!SystemInfo::isHighContrastModeActive()) {
        item->setData(Qt::BackgroundRole, bg.isValid() ? QBrush(bg) : QVariant());
    }
    item->setData(MayChangeBackgroundRole, !group.isEntryImmutable("background-color"));

    const QFont defaultFont = tryToFindFontFor(item);
    if (group.hasKey("font")) {
        const QFont font = group.readEntry("font", defaultFont);
        item->setData(Qt::FontRole, font != defaultFont ? font : QVariant());
        item->setData(HasFontRole,  font != defaultFont);
    } else {
        QFont font = defaultFont;
        font.setStrikeOut(group.readEntry("font-strikeout", false));
        font.setItalic(group.readEntry("font-italic", false));
        font.setBold(group.readEntry("font-bold", false));
        item->setData(Qt::FontRole, font);
        item->setData(HasFontRole, false);
    }
    item->setData(MayChangeFontRole, !group.isEntryImmutable("font"));
    item->setData(MayChangeItalicRole, !group.isEntryImmutable("font-italic"));
    item->setData(MayChangeBoldRole, !group.isEntryImmutable("font-bold"));
    item->setData(MayChangeStrikeOutRole, !group.isEntryImmutable("font-strikeout"));

    const QString iconName = group.readEntry("icon");
    item->setData(Qt::DecorationRole, iconName.isEmpty() ? QVariant() : QIcon::fromTheme(iconName));
    item->setData(IconNameRole, iconName.isEmpty() ? QVariant() : iconName);
    item->setData(MayChangeIconRole, !group.isEntryImmutable("icon"));
}

static void erase_if_allowed(QListWidgetItem *item, int role, int allowRole)
{
    if (item && item->data(allowRole).toBool()) {
        item->setData(role, QVariant());
    }
}

#if 0
static void erase_if_allowed(QListWidgetItem *item, const int role[], size_t numRoles, int allowRole)
{
    if (item && item->data(allowRole).toBool())
        for (unsigned int i = 0; i < numRoles; ++i) {
            item->setData(role[i], QVariant());
        }
}

static void erase_if_allowed(QListWidgetItem *item, int role, const int allowRole[], size_t numAllowRoles)
{
    if (!item) {
        return;
    }
    for (unsigned int i = 0; i < numAllowRoles; ++i)
        if (!item->data(allowRole[i]).toBool()) {
            return;
        }
    item->setData(role, QVariant());
}
#endif

static void erase_if_allowed(QListWidgetItem *item, const int role[], size_t numRoles, const int allowRole[], size_t numAllowRoles)
{
    if (!item) {
        return;
    }
    for (unsigned int i = 0; i < numAllowRoles; ++i)
        if (!item->data(allowRole[i]).toBool()) {
            return;
        }
    for (unsigned int i = 0; i < numRoles; ++i) {
        item->setData(role[i], QVariant());
    }
}

static void set_default_appearance(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    erase_if_allowed(item, StoredForegroundRole, MayChangeForegroundRole);
    erase_if_allowed(item, Qt::ForegroundRole, MayChangeForegroundRole);
    erase_if_allowed(item, StoredBackgroundRole, MayChangeBackgroundRole);
    erase_if_allowed(item, Qt::BackgroundRole, MayChangeBackgroundRole);
    erase_if_allowed(item, Qt::DecorationRole, MayChangeIconRole);
    static const int fontRoles[] = { Qt::FontRole, HasFontRole };
    static const int fontAllowRoles[] = {
        MayChangeFontRole,
        MayChangeItalicRole,
        MayChangeBoldRole,
        MayChangeStrikeOutRole,
    };
    erase_if_allowed(item, fontRoles, sizeof(fontRoles) / sizeof(int), fontAllowRoles, sizeof(fontAllowRoles) / sizeof(int));
}

static void writeOrDelete(KConfigGroup &group, const char *key, const QVariant &value)
{
    if (value.isValid()) {
        group.writeEntry(key, value);
    } else {
        group.deleteEntry(key);
    }
}

static QVariant brush2color(const QVariant &v)
{
    if (v.isValid()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (v.type() == QVariant::Color) {
#else
        if (v.userType() == QMetaType::QColor) {
#endif
            return v;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        } else if (v.type() == QVariant::Brush) {
#else
        } else if (v.userType() == QMetaType::QBrush) {
#endif
            return v.value<QBrush>().color();
        }
    }
    return QVariant();
}

static void save_to_config(const QListWidgetItem *item, KConfigGroup &group)
{
    if (!item) {
        return;
    }
    writeOrDelete(group, "Name", item->data(HasNameRole).toBool() ? item->text() : QVariant());
    writeOrDelete(group, "foreground-color", brush2color(item->data(StoredForegroundRole)));
    writeOrDelete(group, "background-color", brush2color(item->data(StoredBackgroundRole)));
    writeOrDelete(group, "icon", item->data(IconNameRole));

    group.deleteEntry("font");
    group.deleteEntry("font-strikeout");
    group.deleteEntry("font-italic");
    group.deleteEntry("font-bold");

    if (item->data(HasFontRole).toBool()) {
        writeOrDelete(group, "font", item->data(Qt::FontRole));
        return;
    }

    if (is_strikeout(item)) {
        group.writeEntry("font-strikeout", true);
    }
    if (is_italic(item)) {
        group.writeEntry("font-italic", true);
    }
    if (is_bold(item)) {
        group.writeEntry("font-bold", true);
    }
}

static void kiosk_enable(QWidget *w, const QListWidgetItem *item, int allowRole)
{
    if (!w) {
        return;
    }
    if (item && !item->data(allowRole).toBool()) {
        w->setEnabled(false);
        w->setToolTip(i18n("This parameter has been locked down by the system administrator."));
    } else {
        w->setEnabled(item);
        w->setToolTip(QString());
    }
}

class AppearanceConfigWidget::Private : public Ui_AppearanceConfigWidget
{
    friend class ::Kleo::Config::AppearanceConfigWidget;
    AppearanceConfigWidget *const q;
public:
    explicit Private(AppearanceConfigWidget *qq)
        : Ui_AppearanceConfigWidget()
        , q(qq)
    {
        setupUi(q);

        if (QLayout *const l = q->layout()) {
            l->setContentsMargins(0, 0, 0, 0);
        }

        highContrastMsg->setVisible(SystemInfo::isHighContrastModeActive());
        highContrastMsg->setMessageType(KMessageWidget::Warning);
        highContrastMsg->setIcon(q->style()->standardIcon(QStyle::SP_MessageBoxWarning, nullptr, q));
        highContrastMsg->setText(i18n("The preview of colors is disabled because high-contrast mode is active."));
        highContrastMsg->setCloseButtonVisible(false);

        if (Kleo::Settings{}.cmsEnabled()) {
            auto w = new QWidget;
            dnOrderWidget = new DNAttributeOrderConfigWidget{w};
            dnOrderWidget->setObjectName(QStringLiteral("dnOrderWidget"));
            (new QVBoxLayout(w))->addWidget(dnOrderWidget);

            tabWidget->addTab(w, i18n("DN-Attribute Order"));

            connect(dnOrderWidget, &DNAttributeOrderConfigWidget::changed, q, &AppearanceConfigWidget::changed);
        }

        connect(iconButton, SIGNAL(clicked()), q, SLOT(slotIconClicked()));
#ifndef QT_NO_COLORDIALOG
        connect(foregroundButton, SIGNAL(clicked()), q, SLOT(slotForegroundClicked()));
        connect(backgroundButton, SIGNAL(clicked()), q, SLOT(slotBackgroundClicked()));
#else
        foregroundButton->hide();
        backgroundButton->hide();
#endif
#ifndef QT_NO_FONTDIALOG
        connect(fontButton, SIGNAL(clicked()), q, SLOT(slotFontClicked()));
#else
        fontButton->hide();
#endif
        connect(categoriesLV, SIGNAL(itemSelectionChanged()), q, SLOT(slotSelectionChanged()));
        connect(defaultLookPB, SIGNAL(clicked()), q, SLOT(slotDefaultClicked()));
        connect(italicCB, SIGNAL(toggled(bool)), q, SLOT(slotItalicToggled(bool)));
        connect(boldCB, SIGNAL(toggled(bool)), q, SLOT(slotBoldToggled(bool)));
        connect(strikeoutCB, SIGNAL(toggled(bool)), q, SLOT(slotStrikeOutToggled(bool)));
        connect(tooltipValidityCheckBox, SIGNAL(toggled(bool)), q, SLOT(slotTooltipValidityChanged(bool)));
        connect(tooltipOwnerCheckBox, SIGNAL(toggled(bool)), q, SLOT(slotTooltipOwnerChanged(bool)));
        connect(tooltipDetailsCheckBox, SIGNAL(toggled(bool)), q, SLOT(slotTooltipDetailsChanged(bool)));
        connect(useTagsCheckBox, SIGNAL(toggled(bool)), q, SLOT(slotUseTagsChanged(bool)));
    }

private:
    void enableDisableActions(QListWidgetItem *item);
    QListWidgetItem *selectedItem() const;

private:
    void slotIconClicked();
#ifndef QT_NO_COLORDIALOG
    void slotForegroundClicked();
    void slotBackgroundClicked();
#endif
#ifndef QT_NO_FONTDIALOG
    void slotFontClicked();
#endif
    void slotSelectionChanged();
    void slotDefaultClicked();
    void slotItalicToggled(bool);
    void slotBoldToggled(bool);
    void slotStrikeOutToggled(bool);
    void slotTooltipValidityChanged(bool);
    void slotTooltipOwnerChanged(bool);
    void slotTooltipDetailsChanged(bool);
    void slotUseTagsChanged(bool);

private:
    Kleo::DNAttributeOrderConfigWidget *dnOrderWidget = nullptr;
};

AppearanceConfigWidget::AppearanceConfigWidget(QWidget *p, Qt::WindowFlags f)
    : QWidget(p, f), d(new Private(this))
{
//    load();
}

AppearanceConfigWidget::~AppearanceConfigWidget() {}

void AppearanceConfigWidget::Private::slotSelectionChanged()
{
    enableDisableActions(selectedItem());
}

QListWidgetItem *AppearanceConfigWidget::Private::selectedItem() const
{
    const QList<QListWidgetItem *> items = categoriesLV->selectedItems();
    return items.empty() ? nullptr : items.front();
}

void AppearanceConfigWidget::Private::enableDisableActions(QListWidgetItem *item)
{
    kiosk_enable(iconButton, item, MayChangeIconRole);
#ifndef QT_NO_COLORDIALOG
    kiosk_enable(foregroundButton, item, MayChangeForegroundRole);
    kiosk_enable(backgroundButton, item, MayChangeBackgroundRole);
#endif
#ifndef QT_NO_FONTDIALOG
    kiosk_enable(fontButton, item, MayChangeFontRole);
#endif
    kiosk_enable(italicCB, item, MayChangeItalicRole);
    kiosk_enable(boldCB, item, MayChangeBoldRole);
    kiosk_enable(strikeoutCB, item, MayChangeStrikeOutRole);

    defaultLookPB->setEnabled(item);

    italicCB->setChecked(is_italic(item));
    boldCB->setChecked(is_bold(item));
    strikeoutCB->setChecked(is_strikeout(item));
}

void AppearanceConfigWidget::Private::slotDefaultClicked()
{

    QListWidgetItem *const item = selectedItem();
    if (!item) {
        return;
    }

    set_default_appearance(item);
    enableDisableActions(item);

    Q_EMIT q->changed();
}

void AppearanceConfigWidget::defaults()
{
    // This simply means "default look for every category"
    for (int i = 0, end = d->categoriesLV->count(); i != end; ++i) {
        set_default_appearance(d->categoriesLV->item(i));
    }

    // use a temporary TooltipPreferences instance for resetting the values to the defaults;
    // the setters respect the immutability of the individual settings, so that we don't have
    // to check this explicitly
    TooltipPreferences tooltipPrefs;
    tooltipPrefs.setShowValidity(tooltipPrefs.findItem(QStringLiteral("ShowValidity"))->getDefault().toBool());
    d->tooltipValidityCheckBox->setChecked(tooltipPrefs.showValidity());
    tooltipPrefs.setShowOwnerInformation(tooltipPrefs.findItem(QStringLiteral("ShowOwnerInformation"))->getDefault().toBool());
    d->tooltipOwnerCheckBox->setChecked(tooltipPrefs.showOwnerInformation());
    tooltipPrefs.setShowCertificateDetails(tooltipPrefs.findItem(QStringLiteral("ShowCertificateDetails"))->getDefault().toBool());
    d->tooltipDetailsCheckBox->setChecked(tooltipPrefs.showCertificateDetails());

    if (d->dnOrderWidget) {
        const Settings settings;
        if (!settings.isImmutable(QStringLiteral("AttributeOrder"))) {
            d->dnOrderWidget->setAttributeOrder(DN::defaultAttributeOrder());
        }
    }

    Q_EMIT changed();
}

void AppearanceConfigWidget::load()
{
    if (d->dnOrderWidget) {
        const Settings settings;
        d->dnOrderWidget->setAttributeOrder(DN::attributeOrder());
        d->dnOrderWidget->setEnabled(!settings.isImmutable(QStringLiteral("AttributeOrder")));
    }

    d->categoriesLV->clear();
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("libkleopatrarc"));
    if (!config) {
        return;
    }
    const QStringList groups = config->groupList().filter(QRegularExpression(QStringLiteral("^Key Filter #\\d+$")));
    for (const QString &group : groups) {
        const KConfigGroup configGroup{config, group};
        const bool isCmsSpecificKeyFilter = !configGroup.readEntry("is-openpgp-key", true);
        auto item = new QListWidgetItem{d->categoriesLV};
        // hide CMS-specific filters if CMS is disabled; we hide those filters
        // instead of skipping them, so that they are not removed on save
        item->setHidden(isCmsSpecificKeyFilter && !Kleo::Settings{}.cmsEnabled());
        apply_config(configGroup, item);
    }

    const TooltipPreferences prefs;
    d->tooltipValidityCheckBox->setChecked(prefs.showValidity());
    d->tooltipValidityCheckBox->setEnabled(!prefs.isImmutable(QStringLiteral("ShowValidity")));
    d->tooltipOwnerCheckBox->setChecked(prefs.showOwnerInformation());
    d->tooltipOwnerCheckBox->setEnabled(!prefs.isImmutable(QStringLiteral("ShowOwnerInformation")));
    d->tooltipDetailsCheckBox->setChecked(prefs.showCertificateDetails());
    d->tooltipDetailsCheckBox->setEnabled(!prefs.isImmutable(QStringLiteral("ShowCertificateDetails")));

    const TagsPreferences tagsPrefs;
    d->useTagsCheckBox->setChecked(tagsPrefs.useTags());
    d->useTagsCheckBox->setEnabled(!tagsPrefs.isImmutable(QStringLiteral("UseTags")));
}

void AppearanceConfigWidget::save()
{
    if (d->dnOrderWidget) {
        Settings settings;
        settings.setAttributeOrder(d->dnOrderWidget->attributeOrder());
        settings.save();

        DN::setAttributeOrder(settings.attributeOrder());
    }

    TooltipPreferences prefs;
    prefs.setShowValidity(d->tooltipValidityCheckBox->isChecked());
    prefs.setShowOwnerInformation(d->tooltipOwnerCheckBox->isChecked());
    prefs.setShowCertificateDetails(d->tooltipDetailsCheckBox->isChecked());
    prefs.save();

    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("libkleopatrarc"));
    if (!config) {
        return;
    }
    // We know (assume) that the groups in the config object haven't changed,
    // so we just iterate over them and over the listviewitems, and map one-to-one.
    const QStringList groups = config->groupList().filter(QRegularExpression(QStringLiteral("^Key Filter #\\d+$")));
#if 0
    if (groups.isEmpty()) {
        // If we created the default categories ourselves just now, then we need to make up their list
        Q3ListViewItemIterator lvit(categoriesLV);
        for (; lvit.current(); ++lvit) {
            groups << lvit.current()->text(0);
        }
    }
#endif
    for (int i = 0, end = std::min<int>(groups.size(), d->categoriesLV->count()); i != end; ++i) {
        const QListWidgetItem *const item = d->categoriesLV->item(i);
        Q_ASSERT(item);
        KConfigGroup group(config, groups[i]);
        save_to_config(item, group);
    }

    TagsPreferences tagsPrefs;
    tagsPrefs.setUseTags(d->useTagsCheckBox->isChecked());
    tagsPrefs.save();

    config->sync();
    KeyFilterManager::instance()->reload();
}

void AppearanceConfigWidget::Private::slotIconClicked()
{
    QListWidgetItem *const item = selectedItem();
    if (!item) {
        return;
    }

    const QString iconName = KIconDialog::getIcon( /* repeating default arguments begin */
                                 KIconLoader::Desktop, KIconLoader::Application, false, 0, false,
                                 /* repeating default arguments end */
                                 q);
    if (iconName.isEmpty()) {
        return;
    }

    item->setIcon(QIcon::fromTheme(iconName));
    item->setData(IconNameRole, iconName);
    Q_EMIT q->changed();
}

#ifndef QT_NO_COLORDIALOG
void AppearanceConfigWidget::Private::slotForegroundClicked()
{
    QListWidgetItem *const item = selectedItem();
    if (!item) {
        return;
    }

    const QVariant v = brush2color(item->data(StoredForegroundRole));

    const QColor initial = v.isValid() ? v.value<QColor>() : categoriesLV->palette().color(QPalette::Normal, QPalette::Text);
    const QColor c = QColorDialog::getColor(initial, q);

    if (c.isValid()) {
        item->setData(StoredForegroundRole, QBrush(c));
        if (!SystemInfo::isHighContrastModeActive()) {
            item->setData(Qt::ForegroundRole, QBrush(c));
        }
        Q_EMIT q->changed();
    }
}

void AppearanceConfigWidget::Private::slotBackgroundClicked()
{
    QListWidgetItem *const item = selectedItem();
    if (!item) {
        return;
    }

    const QVariant v = brush2color(item->data(StoredBackgroundRole));

    const QColor initial = v.isValid() ? v.value<QColor>() : categoriesLV->palette().color(QPalette::Normal, QPalette::Base);
    const QColor c = QColorDialog::getColor(initial, q);

    if (c.isValid()) {
        item->setData(StoredBackgroundRole, QBrush(c));
        if (!SystemInfo::isHighContrastModeActive()) {
            item->setData(Qt::BackgroundRole, QBrush(c));
        }
        Q_EMIT q->changed();
    }
}
#endif // QT_NO_COLORDIALOG

#ifndef QT_NO_FONTDIALOG
void AppearanceConfigWidget::Private::slotFontClicked()
{
    QListWidgetItem *const item = selectedItem();
    if (!item) {
        return;
    }

    const QVariant v = item->data(Qt::FontRole);

    bool ok = false;
    const QFont defaultFont = tryToFindFontFor(item);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const QFont initial = v.isValid() && v.type() == QVariant::Font ? v.value<QFont>() : defaultFont;
#else
    const QFont initial = v.isValid() && v.userType() == QMetaType::QFont ? v.value<QFont>() : defaultFont;
#endif
    QFont f = QFontDialog::getFont(&ok, initial, q);
    if (!ok) {
        return;
    }

    // disallow circumventing KIOSK:
    if (!item->data(MayChangeItalicRole).toBool()) {
        f.setItalic(initial.italic());
    }
    if (!item->data(MayChangeBoldRole).toBool()) {
        f.setBold(initial.bold());
    }
    if (!item->data(MayChangeStrikeOutRole).toBool()) {
        f.setStrikeOut(initial.strikeOut());
    }

    item->setData(Qt::FontRole, f != defaultFont ? f : QVariant());
    item->setData(HasFontRole, true);
    Q_EMIT q->changed();
}
#endif // QT_NO_FONTDIALOG

void AppearanceConfigWidget::Private::slotItalicToggled(bool on)
{
    set_italic(selectedItem(), on);
    Q_EMIT q->changed();
}

void AppearanceConfigWidget::Private::slotBoldToggled(bool on)
{
    set_bold(selectedItem(), on);
    Q_EMIT q->changed();
}

void AppearanceConfigWidget::Private::slotStrikeOutToggled(bool on)
{
    set_strikeout(selectedItem(), on);
    Q_EMIT q->changed();
}

void AppearanceConfigWidget::Private::slotTooltipValidityChanged(bool)
{
    Q_EMIT q->changed();
}

void AppearanceConfigWidget::Private::slotTooltipOwnerChanged(bool)
{
    Q_EMIT q->changed();
}

void AppearanceConfigWidget::Private::slotTooltipDetailsChanged(bool)
{
    Q_EMIT q->changed();
}

void AppearanceConfigWidget::Private::slotUseTagsChanged(bool)
{
    Q_EMIT q->changed();
}

#include "moc_appearanceconfigwidget.cpp"
