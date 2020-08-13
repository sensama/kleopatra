/* -*- mode: c++; c-basic-offset:4 -*-
    view/keytreeview.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_VIEW_KEYTREEVIEW_H__
#define __KLEOPATRA_VIEW_KEYTREEVIEW_H__

#include <QWidget>

#include <QString>
#include <QStringList>

#include <gpgme++/key.h>

#include <memory>
#include <vector>

#include <KConfigGroup>

class QTreeView;

namespace Kleo
{

class KeyFilter;
class AbstractKeyListModel;
class AbstractKeyListSortFilterProxyModel;
class KeyListSortFilterProxyModel;

class KeyTreeView : public QWidget
{
    Q_OBJECT
public:
    explicit KeyTreeView(QWidget *parent = nullptr);
    KeyTreeView(const QString &stringFilter, const std::shared_ptr<KeyFilter> &keyFilter,
                AbstractKeyListSortFilterProxyModel *additionalProxy, QWidget *parent,
                const KConfigGroup &group = KConfigGroup());
    ~KeyTreeView();

    QTreeView *view() const
    {
        return m_view;
    }

    AbstractKeyListModel *model() const
    {
        return m_isHierarchical ? hierarchicalModel() : flatModel();
    }

    AbstractKeyListModel *flatModel() const
    {
        return m_flatModel;
    }
    AbstractKeyListModel *hierarchicalModel() const
    {
        return m_hierarchicalModel;
    }

    void setFlatModel(AbstractKeyListModel *model);
    void setHierarchicalModel(AbstractKeyListModel *model);

    void setKeys(const std::vector<GpgME::Key> &keys);
    const std::vector<GpgME::Key> &keys() const
    {
        return m_keys;
    }

    void selectKeys(const std::vector<GpgME::Key> &keys);
    std::vector<GpgME::Key> selectedKeys() const;

    void addKeysUnselected(const std::vector<GpgME::Key> &keys);
    void addKeysSelected(const std::vector<GpgME::Key> &keys);
    void removeKeys(const std::vector<GpgME::Key> &keys);

#if 0
    void setToolTipOptions(int options);
    int toolTipOptions() const;
#endif

    QString stringFilter() const
    {
        return m_stringFilter;
    }
    const std::shared_ptr<KeyFilter> &keyFilter() const
    {
        return m_keyFilter;
    }
    bool isHierarchicalView() const
    {
        return m_isHierarchical;
    }

    void setColumnSizes(const std::vector<int> &sizes);
    std::vector<int> columnSizes() const;

    void setSortColumn(int sortColumn, Qt::SortOrder sortOrder);
    int sortColumn() const;
    Qt::SortOrder sortOrder() const;

    virtual KeyTreeView *clone() const
    {
        return new KeyTreeView(*this);
    }

    void disconnectSearchBar(const QObject *bar);
    bool connectSearchBar(const QObject *bar);
    void resizeColumns();

public Q_SLOTS:
    virtual void setStringFilter(const QString &text);
    virtual void setKeyFilter(const std::shared_ptr<Kleo::KeyFilter> &filter);
    virtual void setHierarchicalView(bool on);

Q_SIGNALS:
    void stringFilterChanged(const QString &filter);
    void keyFilterChanged(const std::shared_ptr<Kleo::KeyFilter> &filter);
    void hierarchicalChanged(bool on);

protected:
    KeyTreeView(const KeyTreeView &);

private:
    void init();
    void addKeysImpl(const std::vector<GpgME::Key> &, bool);
    void restoreExpandState();
    void saveLayout();
    void restoreLayout();
    void setupRemarkKeys();

private:
    std::vector<GpgME::Key> m_keys;

    KeyListSortFilterProxyModel *m_proxy;
    AbstractKeyListSortFilterProxyModel *m_additionalProxy;

    QTreeView *m_view;

    AbstractKeyListModel *m_flatModel;
    AbstractKeyListModel *m_hierarchicalModel;

    QString m_stringFilter;
    std::shared_ptr<KeyFilter> m_keyFilter;

    QStringList m_expandedKeys;

    KConfigGroup m_group;

    bool m_isHierarchical : 1;
    bool m_onceResized : 1;
};

}

#endif // __KLEOPATRA_VIEW_KEYTREEVIEW_H__
