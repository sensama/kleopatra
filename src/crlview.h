/*
    crlview.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2001, 2002, 2004 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CRLVIEW_H
#define CRLVIEW_H

#include <QDialog>
#include <QString>
#include <QProcess>

class QTextEdit;
class QPushButton;
class KProcess;
class QTimer;
class QCloseEvent;

class CRLView : public QDialog
{
    Q_OBJECT
public:
    explicit CRLView(QWidget *parent = nullptr);
    ~CRLView();
public Q_SLOTS:
    void slotUpdateView();

protected Q_SLOTS:
    void slotReadStdout();
    void slotProcessExited(int, QProcess::ExitStatus);
    void slotAppendBuffer();

protected:
    void closeEvent(QCloseEvent *);
    void processExited();
private:
    QTextEdit   *_textView;
    QPushButton *_updateButton;
    QPushButton *_closeButton;
    KProcess    *_process;
    QTimer      *_timer;
    QString      _buffer;
};

#endif // CRLVIEW_H
