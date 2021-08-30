/*
    accessibility/accessiblewidgetfactory.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "accessiblewidgetfactory.h"

#include "accessiblerichtextlabel_p.h"
#include "view/urllabel.h"

#include "private/qwidget_p.h"

QAccessibleInterface *Kleo::accessibleWidgetFactory(const QString &classname, QObject *object)
{
    QAccessibleInterface *iface = nullptr;
    if (!object || !object->isWidgetType())
        return iface;

    QWidget *widget = static_cast<QWidget*>(object);
    // QWidget emits destroyed() from its destructor instead of letting the QObject
    // destructor do it, which means the QWidget is unregistered from the accessibility
    // cache. But QWidget destruction also emits enter and leave events, which may end
    // up here, so we have to ensure that we don't fill the cache with an entry of
    // a widget that is going away.
    if (QWidgetPrivate::get(widget)->data.in_destructor)
        return iface;

    if (classname == QString::fromLatin1(Kleo::UrlLabel::staticMetaObject.className())) {
        iface = new AccessibleRichTextLabel{widget};
    }

    return iface;
}
