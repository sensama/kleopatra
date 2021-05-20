/*
    kleopageconfigdialog.h.h

    This file is part of Kleopatra
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-only
*/

#pragma once

#include <KPageDialog>
#include <QList>

class KPageWidgetItem;
class KCModule;

/**
 * KPageDialog based config dialog to be used when
 * KCMUtils are not available. */
class KleoPageConfigDialog : public KPageDialog
{
    Q_OBJECT
public:
    explicit KleoPageConfigDialog(QWidget *parent = nullptr);

    void addModule(const QString &name, const QString &comment, const QString &docPath, const QString &icon, KCModule *module);

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

