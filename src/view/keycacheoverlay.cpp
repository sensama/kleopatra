/*  keycacheoverlay.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2016 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include "keycacheoverlay.h"

#include <Libkleo/KeyCache>

#include "kleopatra_debug.h"

#include <QVBoxLayout>
#include <QProgressBar>
#include <QLabel>
#include <QEvent>
#include <KLocalizedString>

using namespace Kleo;

KeyCacheOverlay::KeyCacheOverlay(QWidget *baseWidget, QWidget *parent)
    : QWidget(parent), mBaseWidget(baseWidget)
{
    const auto cache = KeyCache::instance();

    if (cache->initialized()) {
        // Cache initialized so we are not needed.
        deleteLater();
        return;
    }

    auto vLay = new QVBoxLayout;
    auto bar = new QProgressBar;
    auto label = new QLabel;
    label->setText(QStringLiteral("<h3>%1</h3>").arg(i18n("Loading certificate cache...")));
    bar->setRange(0, 0);
    vLay->addStretch(1);

    auto subLay1 = new QVBoxLayout;
    auto subLay3 = new QHBoxLayout;
    subLay3->addStretch(1);
    subLay3->addWidget(label);
    subLay3->addStretch(1);
    subLay1->addLayout(subLay3);
    subLay1->addWidget(bar);

    auto subLay2 = new QHBoxLayout;
    subLay2->addStretch(0.15);
    subLay2->addLayout(subLay1, 0.7);
    subLay2->addStretch(0.15);

    vLay->addLayout(subLay2);

    vLay->addStretch(1);
    setLayout(vLay);

    connect(cache.get(), &KeyCache::keyListingDone, this, &KeyCacheOverlay::hideOverlay);

    mBaseWidget->installEventFilter(this);
    mBaseWidget->setEnabled(false);
    reposition();
}

bool KeyCacheOverlay::eventFilter(QObject *object, QEvent *event)
{
    if (object == mBaseWidget &&
            (event->type() == QEvent::Move || event->type() == QEvent::Resize ||
             event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
        reposition();
    }
    return QWidget::eventFilter(object, event);
}

void KeyCacheOverlay::reposition()
{
    if (parentWidget() != mBaseWidget->window()) {
        setParent(mBaseWidget->window());
    }
    if (!KeyCache::instance()->initialized()) {
        show();
    }

    const QPoint topLevelPos = mBaseWidget->mapTo(window(), QPoint(0, 0));
    const QPoint parentPos = parentWidget()->mapFrom(window(), topLevelPos);
    move(parentPos);

    resize(mBaseWidget->size());
}

void KeyCacheOverlay::hideOverlay()
{
   mBaseWidget->setEnabled(true);
   hide();
   mBaseWidget->removeEventFilter(this);
   deleteLater();
}
