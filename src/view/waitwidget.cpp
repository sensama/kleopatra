/*  Copyright (c) 2017 Intevation GmbH

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
*/
#include "waitwidget.h"

#include <QProgressBar>
#include <QLabel>
#include <QHBoxLayout>

using namespace Kleo;

WaitWidget::WaitWidget(QWidget *parent)
    : QWidget(parent)
{
    auto vLay = new QVBoxLayout(this);
    auto bar = new QProgressBar;
    mLabel = new QLabel;
    bar->setRange(0, 0);
    vLay->addStretch(1);

    auto subLay1 = new QVBoxLayout;
    auto subLay3 = new QHBoxLayout;
    subLay3->addStretch(1);
    subLay3->addWidget(mLabel);
    subLay3->addStretch(1);
    subLay1->addLayout(subLay3);
    subLay1->addWidget(bar);

    auto subLay2 = new QHBoxLayout;
    subLay2->addStretch(0);
    subLay2->addLayout(subLay1, 0);
    subLay2->addStretch(0);

    vLay->addLayout(subLay2);

    vLay->addStretch(1);
}

void WaitWidget::setText(const QString &text)
{
    mLabel->setText(QStringLiteral("<h3>%1</h3>").arg(text));
}

WaitWidget::~WaitWidget()
{
}
