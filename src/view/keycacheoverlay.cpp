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
#include "waitwidget.h"

#include <QVBoxLayout>
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

    auto vLay = new QVBoxLayout(this);

    auto waitWidget = new WaitWidget(this);

    waitWidget->setText(i18n("Loading certificate cache..."));

    vLay->addWidget(waitWidget);


    mBaseWidget->installEventFilter(this);
    mBaseWidget->setEnabled(false);
    reposition();

    connect(&mTimer, &QTimer::timeout, [this]() {
        // To avoid an infinite show if we miss the keyListingDone signal
        // (Race potential) we use a watchdog timer, too to actively poll
        // the keycache every second. See bug #381910
        if (KeyCache::instance()->initialized()) {
            qCDebug(KLEOPATRA_LOG) << "Hiding overlay from watchdog";
            hideOverlay();
        }
    } );

    mTimer.start(1000);

    connect(cache.get(), &KeyCache::keyListingDone, this, &KeyCacheOverlay::hideOverlay);
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
   mTimer.stop();
   mBaseWidget->setEnabled(true);
   hide();
   mBaseWidget->removeEventFilter(this);
   deleteLater();
}
