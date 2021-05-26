/* -*- mode: c++; c-basic-offset:4 -*-
    mainwindow.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KXmlGuiWindow>

#include <utils/pimpl_ptr.h>

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~MainWindow() override;

public Q_SLOTS:
    void importCertificatesFromFile(const QStringList &files);

protected:
    QByteArray savedGeometry;

    void closeEvent(QCloseEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;
    void readProperties(const KConfigGroup &cg) override;
    void saveProperties(KConfigGroup &cg) override;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void closeAndQuit())
    Q_PRIVATE_SLOT(d, void selfTest())
    Q_PRIVATE_SLOT(d, void configureToolbars())
    Q_PRIVATE_SLOT(d, void configureGroups())
    Q_PRIVATE_SLOT(d, void editKeybindings())
    Q_PRIVATE_SLOT(d, void gnupgLogViewer())
    Q_PRIVATE_SLOT(d, void slotConfigCommitted())
    Q_PRIVATE_SLOT(d, void slotContextMenuRequested(QAbstractItemView *, QPoint))
    Q_PRIVATE_SLOT(d, void slotFocusQuickSearch())
    Q_PRIVATE_SLOT(d, void showCertificateView())
    Q_PRIVATE_SLOT(d, void showPadView())
    Q_PRIVATE_SLOT(d, void showSmartcardView())
    Q_PRIVATE_SLOT(d, void forceUpdateCheck())
    Q_PRIVATE_SLOT(d, void openCompendium())
};

