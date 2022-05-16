/* -*- mode: c++; c-basic-offset:4 -*-
    view/keytreeview.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "keytreeview.h"
#include "searchbar.h"

#include <Libkleo/KeyList>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeyListSortFilterProxyModel>
#include <Libkleo/KeyRearrangeColumnsProxyModel>
#include <Libkleo/Predicates>

#include "utils/headerview.h"
#include "utils/tags.h"

#include <Libkleo/Stl_Util>
#include <Libkleo/KeyFilter>
#include <Libkleo/KeyCache>

#include <gpgme++/key.h>

#include "kleopatra_debug.h"
#include <QTimer>
#include <QTreeView>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QItemSelection>
#include <QLayout>
#include <QList>
#include <QMenu>
#include <QAction>
#include <QEvent>
#include <QContextMenuEvent>
#include <QScrollBar>

#include <KSharedConfig>
#include <KLocalizedString>

#define TAGS_COLUMN 13

using namespace Kleo;
using namespace GpgME;

Q_DECLARE_METATYPE(GpgME::Key)

namespace
{

class TreeView : public QTreeView
{
public:
    explicit TreeView(QWidget *parent = nullptr) : QTreeView(parent)
    {
        header()->installEventFilter(this);
    }

    QSize minimumSizeHint() const override
    {
        const QSize min = QTreeView::minimumSizeHint();
        return QSize(min.width(), min.height() + 5 * fontMetrics().height());
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched)
        if (event->type() == QEvent::ContextMenu) {
            auto e = static_cast<QContextMenuEvent *>(event);

            if (!mHeaderPopup) {
                mHeaderPopup = new QMenu(this);
                mHeaderPopup->setTitle(i18n("View Columns"));
                for (int i = 0; i < model()->columnCount(); ++i) {
                    QAction *tmp
                        = mHeaderPopup->addAction(model()->headerData(i, Qt::Horizontal).toString());
                    tmp->setData(QVariant(i));
                    tmp->setCheckable(true);
                    mColumnActions << tmp;
                }

                connect(mHeaderPopup, &QMenu::triggered, this, [this] (QAction *action) {
                    const int col = action->data().toInt();
                    if ((col == TAGS_COLUMN) && action->isChecked()) {
                        Tags::enableTags();
                    }
                    if (action->isChecked()) {
                        showColumn(col);
                    } else {
                        hideColumn(col);
                    }

                    auto tv = qobject_cast<KeyTreeView *> (parent());
                    if (tv) {
                        tv->resizeColumns();
                    }
                });
            }

            for (QAction *action : std::as_const(mColumnActions)) {
                const int column = action->data().toInt();
                action->setChecked(!isColumnHidden(column));
            }

            mHeaderPopup->popup(mapToGlobal(e->pos()));
            return true;
        }

        return false;
    }

    void keyPressEvent(QKeyEvent *event) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;

private:
    bool mMoveCursorUpdatedView = false;
    QMenu *mHeaderPopup = nullptr;

    QList<QAction *> mColumnActions;
};

void TreeView::keyPressEvent(QKeyEvent *event)
{
    mMoveCursorUpdatedView = false;
    QTreeView::keyPressEvent(event);
    if (mMoveCursorUpdatedView) {
        event->accept();
    }
}

QModelIndex TreeView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    // this code is based heavily on QTreeView::moveCursor()

    QModelIndex current = currentIndex();
    if (!current.isValid()) {
        // let QTreeView handle invalid current index
        return QTreeView::moveCursor(cursorAction, modifiers);
    }

    if (isRightToLeft()) {
        if (cursorAction == MoveRight) {
            cursorAction = MoveLeft;
        } else if (cursorAction == MoveLeft) {
            cursorAction = MoveRight;
        }
    }
    switch (cursorAction) {
    case MoveLeft: {
        // HACK: call QTreeView::moveCursor with invalid cursor action to make it call the private executePostedLayout()
        (void) QTreeView::moveCursor(static_cast<CursorAction>(-1), modifiers);

        int visualColumn = header()->visualIndex(current.column()) - 1;
        while (visualColumn >= 0 && isColumnHidden(header()->logicalIndex(visualColumn))) {
            visualColumn--;
        }
        int newColumn = header()->logicalIndex(visualColumn);
        QModelIndex next = current.sibling(current.row(), newColumn);
        if (next.isValid()) {
            return next;
        }

        //last restort: we change the scrollbar value
        QScrollBar *sb = horizontalScrollBar();
        int oldValue = sb->value();
        sb->setValue(sb->value() - sb->singleStep());
        if (oldValue != sb->value()) {
            mMoveCursorUpdatedView = true;
        }

        updateGeometries();
        viewport()->update();
        break;
    }
    case MoveRight: {
        // HACK: call QTreeView::moveCursor with invalid cursor action to make it call the private executePostedLayout()
        (void) QTreeView::moveCursor(static_cast<CursorAction>(-1), modifiers);

        int visualColumn = header()->visualIndex(current.column()) + 1;
        while (visualColumn < model()->columnCount(current.parent()) && isColumnHidden(header()->logicalIndex(visualColumn))) {
            visualColumn++;
        }
        const int newColumn = header()->logicalIndex(visualColumn);
        const QModelIndex next = current.sibling(current.row(), newColumn);
        if (next.isValid()) {
            return next;
        }

        //last restort: we change the scrollbar value
        QScrollBar *sb = horizontalScrollBar();
        int oldValue = sb->value();
        sb->setValue(sb->value() + sb->singleStep());
        if (oldValue != sb->value()) {
            mMoveCursorUpdatedView = true;
        }

        updateGeometries();
        viewport()->update();
        break;
    }
    default:
        return QTreeView::moveCursor(cursorAction, modifiers);
    }

    return current;
}

const KeyListModelInterface * keyListModel(const QTreeView &view)
{
    const KeyListModelInterface *const klmi = dynamic_cast<KeyListModelInterface *>(view.model());
    Q_ASSERT(klmi);
    return klmi;
}

} // anon namespace

KeyTreeView::KeyTreeView(QWidget *parent)
    : QWidget(parent),
      m_proxy(new KeyListSortFilterProxyModel(this)),
      m_additionalProxy(nullptr),
      m_view(new TreeView(this)),
      m_flatModel(nullptr),
      m_hierarchicalModel(nullptr),
      m_stringFilter(),
      m_keyFilter(),
      m_isHierarchical(true)
{
    init();
}

KeyTreeView::KeyTreeView(const KeyTreeView &other)
    : QWidget(nullptr),
      m_proxy(new KeyListSortFilterProxyModel(this)),
      m_additionalProxy(other.m_additionalProxy ? other.m_additionalProxy->clone() : nullptr),
      m_view(new TreeView(this)),
      m_flatModel(other.m_flatModel),
      m_hierarchicalModel(other.m_hierarchicalModel),
      m_stringFilter(other.m_stringFilter),
      m_keyFilter(other.m_keyFilter),
      m_group(other.m_group),
      m_isHierarchical(other.m_isHierarchical)
{
    init();
    setColumnSizes(other.columnSizes());
    setSortColumn(other.sortColumn(), other.sortOrder());
}

KeyTreeView::KeyTreeView(const QString &text, const std::shared_ptr<KeyFilter> &kf,
                         AbstractKeyListSortFilterProxyModel *proxy, QWidget *parent,
                         const KConfigGroup &group)
    : QWidget(parent),
      m_proxy(new KeyListSortFilterProxyModel(this)),
      m_additionalProxy(proxy),
      m_view(new TreeView(this)),
      m_flatModel(nullptr),
      m_hierarchicalModel(nullptr),
      m_stringFilter(text),
      m_keyFilter(kf),
      m_group(group),
      m_isHierarchical(true),
      m_onceResized(false)
{
    init();
}

void KeyTreeView::setColumnSizes(const std::vector<int> &sizes)
{
    if (sizes.empty()) {
        return;
    }
    Q_ASSERT(m_view);
    Q_ASSERT(m_view->header());
    Q_ASSERT(qobject_cast<HeaderView *>(m_view->header()) == static_cast<HeaderView *>(m_view->header()));
    if (auto const hv = static_cast<HeaderView *>(m_view->header())) {
        hv->setSectionSizes(sizes);
    }
}

void KeyTreeView::setSortColumn(int sortColumn, Qt::SortOrder sortOrder)
{
    Q_ASSERT(m_view);
    m_view->sortByColumn(sortColumn, sortOrder);
}

int KeyTreeView::sortColumn() const
{
    Q_ASSERT(m_view);
    Q_ASSERT(m_view->header());
    return m_view->header()->sortIndicatorSection();
}

Qt::SortOrder KeyTreeView::sortOrder() const
{
    Q_ASSERT(m_view);
    Q_ASSERT(m_view->header());
    return m_view->header()->sortIndicatorOrder();
}

std::vector<int> KeyTreeView::columnSizes() const
{
    Q_ASSERT(m_view);
    Q_ASSERT(m_view->header());
    Q_ASSERT(qobject_cast<HeaderView *>(m_view->header()) == static_cast<HeaderView *>(m_view->header()));
    if (auto const hv = static_cast<HeaderView *>(m_view->header())) {
        return hv->sectionSizes();
    } else {
        return std::vector<int>();
    }
}

void KeyTreeView::init()
{
    KDAB_SET_OBJECT_NAME(m_proxy);
    KDAB_SET_OBJECT_NAME(m_view);

    if (m_group.isValid()) {
        // Reopen as non const
        KConfig *conf = m_group.config();
        m_group = conf->group(m_group.name());
    }

    if (m_additionalProxy && m_additionalProxy->objectName().isEmpty()) {
        KDAB_SET_OBJECT_NAME(m_additionalProxy);
    }
    QLayout *layout = new QVBoxLayout(this);
    KDAB_SET_OBJECT_NAME(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    auto headerView = new HeaderView(Qt::Horizontal);
    KDAB_SET_OBJECT_NAME(headerView);
    headerView->installEventFilter(m_view);
    headerView->setSectionsMovable(true);
    m_view->setHeader(headerView);

    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setAllColumnsShowFocus(false);
    m_view->setSortingEnabled(true);
    m_view->setAccessibleName(i18n("Certificates"));
    m_view->setAccessibleDescription(m_isHierarchical ? i18n("Hierarchical list of certificates") : i18n("List of certificates"));

    if (model()) {
        if (m_additionalProxy) {
            m_additionalProxy->setSourceModel(model());
        } else {
            m_proxy->setSourceModel(model());
        }
    }
    if (m_additionalProxy) {
        m_proxy->setSourceModel(m_additionalProxy);
        if (!m_additionalProxy->parent()) {
            m_additionalProxy->setParent(this);
        }
    }

    m_proxy->setFilterFixedString(m_stringFilter);
    m_proxy->setKeyFilter(m_keyFilter);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    auto rearangingModel = new KeyRearrangeColumnsProxyModel(this);
    rearangingModel->setSourceModel(m_proxy);
    rearangingModel->setSourceColumns(QVector<int>() << KeyList::PrettyName
                                                     << KeyList::PrettyEMail
                                                     << KeyList::Validity
                                                     << KeyList::ValidFrom
                                                     << KeyList::ValidUntil
                                                     << KeyList::TechnicalDetails
                                                     << KeyList::KeyID
                                                     << KeyList::Fingerprint
                                                     << KeyList::OwnerTrust
                                                     << KeyList::Origin
                                                     << KeyList::LastUpdate
                                                     << KeyList::Issuer
                                                     << KeyList::SerialNumber
    // If a column is added before this TAGS_COLUMN define has to be updated accordingly
                                                     << KeyList::Remarks
    );
    m_view->setModel(rearangingModel);

    /* Handle expansion state */
    if (m_group.isValid()) {
        m_expandedKeys = m_group.readEntry("Expanded", QStringList());
    }

    connect(m_view, &QTreeView::expanded, this, [this] (const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        const auto &key = index.data(KeyList::KeyRole).value<GpgME::Key>();
        if (key.isNull()) {
            return;
        }
        const auto fpr = QString::fromLatin1(key.primaryFingerprint());

        if (m_expandedKeys.contains(fpr)) {
            return;
        }
        m_expandedKeys << fpr;
        if (m_group.isValid()) {
            m_group.writeEntry("Expanded", m_expandedKeys);
        }
    });

    connect(m_view, &QTreeView::collapsed, this, [this] (const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        const auto &key = index.data(KeyList::KeyRole).value<GpgME::Key>();
        if (key.isNull()) {
            return;
        }
        m_expandedKeys.removeAll(QString::fromLatin1(key.primaryFingerprint()));
        if (m_group.isValid()) {
            m_group.writeEntry("Expanded", m_expandedKeys);
        }
    });

    connect(KeyCache::instance().get(), &KeyCache::keysMayHaveChanged, this, [this] () {
        /* We use a single shot timer here to ensure that the keysMayHaveChanged
         * handlers are all handled before we restore the expand state so that
         * the model is already populated. */
        QTimer::singleShot(0, [this] () {
            restoreExpandState();
            setUpTagKeys();
            if (!m_onceResized) {
                m_onceResized = true;
                resizeColumns();
            }
        });
    });
    resizeColumns();
    if (m_group.isValid()) {
        restoreLayout(m_group);
    }
}

void KeyTreeView::restoreExpandState()
{
    if (!KeyCache::instance()->initialized()) {
        qCWarning(KLEOPATRA_LOG) << "Restore expand state before keycache available. Aborting.";
        return;
    }
    for (const auto &fpr: std::as_const(m_expandedKeys)) {
        const KeyListModelInterface *const km = keyListModel(*m_view);
        if (!km) {
            qCWarning(KLEOPATRA_LOG) << "invalid model";
            return;
        }
        const auto key = KeyCache::instance()->findByFingerprint(fpr.toLatin1().constData());
        if (key.isNull()) {
            qCDebug(KLEOPATRA_LOG) << "Cannot find:" << fpr << "anymore in cache";
            m_expandedKeys.removeAll(fpr);
            return;
        }
        const auto idx = km->index(key);
        if (!idx.isValid()) {
            qCDebug(KLEOPATRA_LOG) << "Cannot find:" << fpr << "anymore in model";
            m_expandedKeys.removeAll(fpr);
            return;
        }
        m_view->expand(idx);
    }
}

void KeyTreeView::setUpTagKeys()
{
    const auto tagKeys = Tags::tagKeys();
    if (m_hierarchicalModel) {
        m_hierarchicalModel->setRemarkKeys(tagKeys);
    }
    if (m_flatModel) {
        m_flatModel->setRemarkKeys(tagKeys);
    }
}

void KeyTreeView::saveLayout(KConfigGroup &group)
{
    QHeaderView *header = m_view->header();

    QVariantList columnVisibility;
    QVariantList columnOrder;
    QVariantList columnWidths;
    const int headerCount = header->count();
    columnVisibility.reserve(headerCount);
    columnWidths.reserve(headerCount);
    columnOrder.reserve(headerCount);
    for (int i = 0; i < headerCount; ++i) {
        columnVisibility << QVariant(!m_view->isColumnHidden(i));
        columnWidths << QVariant(header->sectionSize(i));
        columnOrder << QVariant(header->visualIndex(i));
    }

    group.writeEntry("ColumnVisibility", columnVisibility);
    group.writeEntry("ColumnOrder", columnOrder);
    group.writeEntry("ColumnWidths", columnWidths);

    group.writeEntry("SortAscending", (int)header->sortIndicatorOrder());
    if (header->isSortIndicatorShown()) {
        group.writeEntry("SortColumn", header->sortIndicatorSection());
    } else {
        group.writeEntry("SortColumn", -1);
    }
}

void KeyTreeView::restoreLayout(const KConfigGroup &group)
{
    QHeaderView *header = m_view->header();

    QVariantList columnVisibility = group.readEntry("ColumnVisibility", QVariantList());
    QVariantList columnOrder = group.readEntry("ColumnOrder", QVariantList());
    QVariantList columnWidths = group.readEntry("ColumnWidths", QVariantList());

    if (columnVisibility.isEmpty()) {
        // if config is empty then use default settings
        // The numbers have to be in line with the order in
        // setsSourceColumns above
        m_view->hideColumn(5);

        for (int i = 7; i < m_view->model()->columnCount(); ++i) {
            m_view->hideColumn(i);
        }
        if (KeyCache::instance()->initialized()) {
            QTimer::singleShot(0, this, &KeyTreeView::resizeColumns);
        }
    } else {
        for (int i = 0; i < header->count(); ++i) {
            if (i >= columnOrder.size() || i >= columnWidths.size() || i >= columnVisibility.size()) {
                // An additional column that was not around last time we saved.
                // We default to hidden.
                m_view->hideColumn(i);
                continue;
            }
            bool visible = columnVisibility[i].toBool();
            int width = columnWidths[i].toInt();
            int order = columnOrder[i].toInt();

            header->resizeSection(i, width ? width : 100);
            header->moveSection(header->visualIndex(i), order);
            if ((i == TAGS_COLUMN) && visible) {
                Tags::enableTags();
            }
            if (!visible) {
                m_view->hideColumn(i);
            }
        }
        m_onceResized = true;
    }

    int sortOrder = group.readEntry("SortAscending", (int)Qt::AscendingOrder);
    int sortColumn = group.readEntry("SortColumn", 0);
    if (sortColumn >= 0) {
        m_view->sortByColumn(sortColumn, (Qt::SortOrder)sortOrder);
    }
}

KeyTreeView::~KeyTreeView()
{
    if (m_group.isValid()) {
        saveLayout(m_group);
    }
}

static QAbstractProxyModel *find_last_proxy(QAbstractProxyModel *pm)
{
    Q_ASSERT(pm);
    while (auto const sm = qobject_cast<QAbstractProxyModel *>(pm->sourceModel())) {
        pm = sm;
    }
    return pm;
}

void KeyTreeView::setFlatModel(AbstractKeyListModel *model)
{
    if (model == m_flatModel) {
        return;
    }
    m_flatModel = model;
    if (!m_isHierarchical)
        // TODO: this fails when called after setHierarchicalView( false )...
    {
        find_last_proxy(m_proxy)->setSourceModel(model);
    }
}

void KeyTreeView::setHierarchicalModel(AbstractKeyListModel *model)
{
    if (model == m_hierarchicalModel) {
        return;
    }
    m_hierarchicalModel = model;
    if (m_isHierarchical) {
        find_last_proxy(m_proxy)->setSourceModel(model);
        m_view->expandAll();
        for (int column = 0; column < m_view->header()->count(); ++column) {
            m_view->header()->resizeSection(column, qMax(m_view->header()->sectionSize(column), m_view->header()->sectionSizeHint(column)));
        }
    }
}

void KeyTreeView::setStringFilter(const QString &filter)
{
    if (filter == m_stringFilter) {
        return;
    }
    m_stringFilter = filter;
    m_proxy->setFilterFixedString(filter);
    Q_EMIT stringFilterChanged(filter);
}

void KeyTreeView::setKeyFilter(const std::shared_ptr<KeyFilter> &filter)
{
    if (filter == m_keyFilter || (filter && m_keyFilter && filter->id() == m_keyFilter->id())) {
        return;
    }
    m_keyFilter = filter;
    m_proxy->setKeyFilter(filter);
    Q_EMIT keyFilterChanged(filter);
}

namespace
{
QItemSelection itemSelectionFromKeys(const std::vector<Key> &keys, const QTreeView &view)
{
    const QModelIndexList indexes = keyListModel(view)->indexes(keys);
    return std::accumulate(
        indexes.cbegin(), indexes.cend(),
        QItemSelection(),
        [] (QItemSelection &selection, const QModelIndex &index) {
            if (index.isValid()) {
                selection.merge(QItemSelection(index, index), QItemSelectionModel::Select);
            }
            return selection;
        });
}
}

void KeyTreeView::selectKeys(const std::vector<Key> &keys)
{
    m_view->selectionModel()->select(itemSelectionFromKeys(keys, *m_view),
                                     QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

std::vector<Key> KeyTreeView::selectedKeys() const
{
    return keyListModel(*m_view)->keys(m_view->selectionModel()->selectedRows());
}

void KeyTreeView::setHierarchicalView(bool on)
{
    if (on == m_isHierarchical) {
        return;
    }
    if (on && !hierarchicalModel()) {
        qCWarning(KLEOPATRA_LOG) <<  "hierarchical view requested, but no hierarchical model set";
        return;
    }
    if (!on && !flatModel()) {
        qCWarning(KLEOPATRA_LOG) << "flat view requested, but no flat model set";
        return;
    }
    const std::vector<Key> selectedKeys = this->selectedKeys();
    const Key currentKey = keyListModel(*m_view)->key(m_view->currentIndex());

    m_isHierarchical = on;
    find_last_proxy(m_proxy)->setSourceModel(model());
    if (on) {
        m_view->expandAll();
    }
    selectKeys(selectedKeys);
    if (!currentKey.isNull()) {
        const QModelIndex currentIndex = keyListModel(*m_view)->index(currentKey);
        if (currentIndex.isValid()) {
            m_view->selectionModel()->setCurrentIndex(currentIndex, QItemSelectionModel::NoUpdate);
            m_view->scrollTo(currentIndex);
        }
    }
    m_view->setAccessibleDescription(m_isHierarchical ? i18n("Hierarchical list of certificates") : i18n("List of certificates"));
    Q_EMIT hierarchicalChanged(on);
}

void KeyTreeView::setKeys(const std::vector<Key> &keys)
{
    std::vector<Key> sorted = keys;
    _detail::sort_by_fpr(sorted);
    _detail::remove_duplicates_by_fpr(sorted);
    m_keys = sorted;
    if (m_flatModel) {
        m_flatModel->setKeys(sorted);
    }
    if (m_hierarchicalModel) {
        m_hierarchicalModel->setKeys(sorted);
    }
}

void KeyTreeView::addKeysImpl(const std::vector<Key> &keys, bool select)
{
    if (keys.empty()) {
        return;
    }
    if (m_keys.empty()) {
        setKeys(keys);
        return;
    }

    std::vector<Key> sorted = keys;
    _detail::sort_by_fpr(sorted);
    _detail::remove_duplicates_by_fpr(sorted);

    std::vector<Key> newKeys = _detail::union_by_fpr(sorted, m_keys);
    m_keys.swap(newKeys);

    if (m_flatModel) {
        m_flatModel->addKeys(sorted);
    }
    if (m_hierarchicalModel) {
        m_hierarchicalModel->addKeys(sorted);
    }

    if (select) {
        selectKeys(sorted);
    }
}

void KeyTreeView::addKeysSelected(const std::vector<Key> &keys)
{
    addKeysImpl(keys, true);
}

void KeyTreeView::addKeysUnselected(const std::vector<Key> &keys)
{
    addKeysImpl(keys, false);
}

void KeyTreeView::removeKeys(const std::vector<Key> &keys)
{
    if (keys.empty()) {
        return;
    }
    std::vector<Key> sorted = keys;
    _detail::sort_by_fpr(sorted);
    _detail::remove_duplicates_by_fpr(sorted);
    std::vector<Key> newKeys;
    newKeys.reserve(m_keys.size());
    std::set_difference(m_keys.begin(), m_keys.end(),
                        sorted.begin(), sorted.end(),
                        std::back_inserter(newKeys),
                        _detail::ByFingerprint<std::less>());
    m_keys.swap(newKeys);

    if (m_flatModel) {
        std::for_each(sorted.cbegin(), sorted.cend(),
                      [this](const Key &key) { m_flatModel->removeKey(key); });
    }
    if (m_hierarchicalModel) {
        std::for_each(sorted.cbegin(), sorted.cend(),
                      [this](const Key &key) { m_hierarchicalModel->removeKey(key); });
    }

}

void KeyTreeView::disconnectSearchBar()
{
    for (const auto &connection : m_connections) {
        disconnect(connection);
    }
    m_connections.clear();
}

bool KeyTreeView::connectSearchBar(const SearchBar *bar)
{
    m_connections.reserve(4);
    m_connections.push_back(connect(this, &KeyTreeView::stringFilterChanged, bar, &SearchBar::setStringFilter));
    m_connections.push_back(connect(bar, &SearchBar::stringFilterChanged, this, &KeyTreeView::setStringFilter));
    m_connections.push_back(connect(this, &KeyTreeView::keyFilterChanged, bar, &SearchBar::setKeyFilter));
    m_connections.push_back(connect(bar, &SearchBar::keyFilterChanged, this, &KeyTreeView::setKeyFilter));

    return std::all_of(m_connections.cbegin(), m_connections.cend(), [](const QMetaObject::Connection &conn) {
        return conn;
    });
}

void KeyTreeView::resizeColumns()
{
    m_view->setColumnWidth(KeyList::PrettyName, 260);
    m_view->setColumnWidth(KeyList::PrettyEMail, 260);

    for (int i = 2; i < m_view->model()->columnCount(); ++i) {
        m_view->resizeColumnToContents(i);
    }
}
