/*  progressoverlay.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileCopyrightText: 2024 g10 Code GmbH
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "progressoverlay.h"

#include "kleopatra_debug.h"
#include "waitwidget.h"

#include <QEvent>
#include <QVBoxLayout>

using namespace Kleo;

ProgressOverlay::ProgressOverlay(QWidget *baseWidget, QWidget *parent)
    : QWidget(parent)
    , mBaseWidget(baseWidget)
{
    auto vLay = new QVBoxLayout(this);

    mWaitWidget = new WaitWidget(this);

    vLay->addWidget(mWaitWidget);
}

ProgressOverlay::~ProgressOverlay()
{
}

void ProgressOverlay::setText(const QString &text)
{
    mWaitWidget->setText(text);
}

QString ProgressOverlay::text() const
{
    return mWaitWidget->text();
}

void ProgressOverlay::showOverlay()
{
    if (shown) {
        return;
    }
    shown = true;
    mBaseWidget->setEnabled(false);
    reposition();
    mBaseWidget->installEventFilter(this);
}

void ProgressOverlay::hideOverlay()
{
    if (!shown) {
        return;
    }
    shown = false;
    mBaseWidget->removeEventFilter(this);
    hide();
    mBaseWidget->setEnabled(true);
}

bool ProgressOverlay::eventFilter(QObject *object, QEvent *event)
{
    if (object == mBaseWidget
        && (event->type() == QEvent::Move || event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
        reposition();
    }
    return QWidget::eventFilter(object, event);
}

void ProgressOverlay::reposition()
{
    if (parentWidget() != mBaseWidget->window()) {
        setParent(mBaseWidget->window());
    }
    show();

    const QPoint topLevelPos = mBaseWidget->mapTo(window(), QPoint(0, 0));
    const QPoint parentPos = parentWidget()->mapFrom(window(), topLevelPos);
    move(parentPos);

    resize(mBaseWidget->size());
}

#include "moc_progressoverlay.cpp"
