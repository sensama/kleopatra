/*
    main.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2001, 2002, 2004 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TRAY_H
#define TRAY_H

#include <QIcon>
#include <KStatusNotifierItem>

class KWatchGnuPGMainWindow;

class KWatchGnuPGTray : public KStatusNotifierItem
{
    Q_OBJECT
public:
    explicit KWatchGnuPGTray(KWatchGnuPGMainWindow *mainwin);
    virtual ~KWatchGnuPGTray();

public Q_SLOTS:
    void setAttention(bool att);
private:
    QIcon mNormalPix;
    QIcon mAttentionPix;
};

#endif /* TRAY_H */

