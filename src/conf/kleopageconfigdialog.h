/*
    kleopageconfigdialog.h.h

    This file is part of Kleopatra
    Copyright (c) 2016 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

    Kleopatra is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License,
    version 2, as published by the Free Software Foundation.

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

#ifndef __KLEOPATRA_CONF_KLEOPAGECONFIGDIALOG_H__
#define __KLEOPATRA_CONF_KLEOPAGECONFIGDIALOG_H__

#include <KPageDialog>
#include <QList>

class KCModule;
class KPageWidgetItem;

/**
 * KPageDialog based config dialog to be used when
 * KCMUtils are not available. */
class KleoPageConfigDialog : public KPageDialog
{
    Q_OBJECT
public:
    explicit KleoPageConfigDialog(QWidget *parent = nullptr);

    void addModule(const QString &module);

Q_SIGNALS:
    void configCommitted();

protected Q_SLOTS:
    void slotDefaultClicked();
    void slotUser1Clicked();
    void slotApplyClicked();
    void slotOkClicked();
    void slotHelpClicked();
    void slotCurrentPageChanged(KPageWidgetItem *current, KPageWidgetItem *previous);
    void moduleChanged(bool value);

private:
    void clientChanged();
    void apply();

    QList<KCModule *> mModules;
    QList<KCModule *> mChangedModules;
    QMap<QString, QString> mHelpUrls;
};

#endif /* __KLEOPATRA_CONF_KLEOPAGECONFIGDIALOG_H__ */
