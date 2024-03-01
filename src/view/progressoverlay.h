/*  progressoverlay.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileCopyrightText: 2024 g10 Code GmbH
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QWidget>

namespace Kleo
{
class WaitWidget;

/**
 * @internal
 * Overlay widget to block another widget while an operation is in progress.
 */
class ProgressOverlay : public QWidget
{
    Q_OBJECT
public:
    /**
     * Create an overlay widget for @p baseWidget.
     * @p baseWidget must not be @c nullptr.
     * @p parent must not be equal to @p baseWidget
     */
    explicit ProgressOverlay(QWidget *baseWidget, QWidget *parent = nullptr);
    ~ProgressOverlay() override;

    void setText(const QString &text);
    QString text() const;

public Q_SLOTS:
    void showOverlay();
    void hideOverlay();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void reposition();

private:
    bool shown = false;
    QWidget *mBaseWidget = nullptr;
    WaitWidget *mWaitWidget = nullptr;
};

} // namespace Kleo
