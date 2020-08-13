/* -*- mode: c++; c-basic-offset:4 -*-
    systemtrayicon.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007, 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_UTILS_SYSTEMTRAYICON_H__
#define __KLEOPATRA_UTILS_SYSTEMTRAYICON_H__

#include <QSystemTrayIcon>

#ifndef QT_NO_SYSTEMTRAYICON

#include <utils/pimpl_ptr.h>

namespace Kleo
{

class SystemTrayIcon : public QSystemTrayIcon
{
    Q_OBJECT
public:
    explicit SystemTrayIcon(QObject *parent = nullptr);
    explicit SystemTrayIcon(const QIcon &icon, QObject *parent = nullptr);
    ~SystemTrayIcon() override;

    void setMainWindow(QWidget *w);
    QWidget *mainWindow() const;

    void setAttentionWindow(QWidget *w);
    QWidget *attentionWindow() const;

    QIcon attentionIcon() const;
    QIcon normalIcon() const;
    bool attentionWanted() const;

public Q_SLOTS:
    void setAttentionIcon(const QIcon &icon);
    void setNormalIcon(const QIcon &icon);
    void setAttentionWanted(bool);

protected Q_SLOTS:
    virtual void slotEnableDisableActions() = 0;

private:
    virtual void doMainWindowSet(QWidget *);
    virtual void doMainWindowClosed(QWidget *);
    virtual void doAttentionWindowClosed(QWidget *);
    virtual void doActivated() = 0;

private:
    bool eventFilter(QObject *, QEvent *) override;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void slotAttentionAnimationTimerTimout())
    Q_PRIVATE_SLOT(d, void slotActivated(QSystemTrayIcon::ActivationReason))
};

} // namespace Kleo

#endif // QT_NO_SYSTEMTRAYICON

#endif /* __KLEOPATRA_UTILS_SYSTEMTRAYICON_H__ */
