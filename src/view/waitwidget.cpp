/*  SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "waitwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>

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

#include "moc_waitwidget.cpp"
