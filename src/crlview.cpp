/*
    crlview.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2001, 2002, 2004 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "crlview.h"

#include <KLocalizedString>
#include <KProcess>
#include <KMessageBox>
#include <QPushButton>
#include <KStandardGuiItem>

#include <QLabel>
#include <QTextEdit>
#include <QTimer>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QFontDatabase>
#include <KGuiItem>

CRLView::CRLView(QWidget *parent)
    : QDialog(parent), _process(0)
{
    QVBoxLayout *topLayout = new QVBoxLayout(this);
    topLayout->setSpacing(4);
    topLayout->setContentsMargins(10, 10, 10, 10);

    topLayout->addWidget(new QLabel(i18n("CRL cache dump:"), this));

    _textView = new QTextEdit(this);
    _textView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    _textView->setReadOnly(true);
    topLayout->addWidget(_textView);

    QHBoxLayout *hbLayout = new QHBoxLayout();
    topLayout->addItem(hbLayout);

    _updateButton = new QPushButton(i18n("&Update"), this);
    _closeButton = new QPushButton(this);
    KGuiItem::assign(_closeButton, KStandardGuiItem::close());

    hbLayout->addWidget(_updateButton);
    hbLayout->addStretch();
    hbLayout->addWidget(_closeButton);

    // connections:
    connect(_updateButton, &QPushButton::clicked, this, &CRLView::slotUpdateView);
    connect(_closeButton, &QPushButton::clicked, this, &CRLView::close);

    resize(_textView->fontMetrics().width('M') * 80,
           _textView->fontMetrics().lineSpacing() * 25);

    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, this, &CRLView::slotAppendBuffer);
}

CRLView::~CRLView()
{
    delete _process; _process = nullptr;
}

void CRLView::closeEvent(QCloseEvent *e)
{
    QDialog::closeEvent(e);
    delete _process; _process = nullptr;
}

void CRLView::slotUpdateView()
{
    _updateButton->setEnabled(false);
    _textView->clear();
    _buffer.clear();
    if (!_process) {
        _process = new KProcess();
        *_process << "gpgsm" << "--call-dirmngr" << "listcrls";
        connect(_process, &KProcess::readyReadStandardOutput, this, &CRLView::slotReadStdout);
        connect(_process, &KProcess::finished, this, &CRLView::slotProcessExited);
    }
    if (_process->state() == QProcess::Running) {
        _process->kill();
    }
    _process->setOutputChannelMode(KProcess::OnlyStdoutChannel);
    _process->start();
    if (!_process->waitForStarted()) {
        KMessageBox::error(this, i18n("Unable to start gpgsm process. Please check your installation."), i18n("Certificate Manager Error"));
        processExited();
    }
    _timer->start(1000);
}

void CRLView::slotReadStdout()
{
    _buffer.append(_process->readAllStandardOutput());
}

void CRLView::slotAppendBuffer()
{
    _textView->append(_buffer);
    _buffer.clear();
}

void CRLView::processExited()
{
    _timer->stop();
    slotAppendBuffer();
    _updateButton->setEnabled(true);
}

void CRLView::slotProcessExited(int, QProcess::ExitStatus _status)
{
    processExited();
    if (_status != QProcess::NormalExit) {
        KMessageBox::error(this, i18n("The GpgSM process ended prematurely because of an unexpected error."), i18n("Certificate Manager Error"));
    }
}

