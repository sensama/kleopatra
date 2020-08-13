/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/listwidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_NEWCERTIFICATEWIZARD_LISTWIDGET_H__
#define __KLEOPATRA_NEWCERTIFICATEWIZARD_LISTWIDGET_H__

#include <QWidget>

#include <utils/pimpl_ptr.h>

class QRegExp;
class QString;
class QStringList;

namespace Kleo
{
namespace NewCertificateUi
{

class ListWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList items READ items WRITE setItems USER true NOTIFY itemsChanged)
    Q_PROPERTY(QRegExp regExpFilter READ regExpFilter WRITE setRegExpFilter)
    Q_PROPERTY(QString defaultValue READ defaultValue WRITE setDefaultValue)
public:
    explicit ListWidget(QWidget *parent = nullptr);
    ~ListWidget();

    void setDefaultValue(const QString &defaultValue);
    QString defaultValue() const;

    void setRegExpFilter(const QRegExp &rx);
    QRegExp regExpFilter() const;

    QStringList items() const;

public Q_SLOTS:
    void setItems(const QStringList &items);

Q_SIGNALS:
    void itemsChanged();

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotAdd())
    Q_PRIVATE_SLOT(d, void slotRemove())
    Q_PRIVATE_SLOT(d, void slotUp())
    Q_PRIVATE_SLOT(d, void slotDown())
    Q_PRIVATE_SLOT(d, void slotSelectionChanged())
};

}
}

#endif /* __KLEOPATRA_NEWCERTIFICATEWIZARD_LISTWIDGET_H__ */
