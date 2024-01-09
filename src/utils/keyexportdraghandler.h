// SPDX-FileCopyrightText: 2024 g10 Code GmbH
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Libkleo/KeyListModel>

#include <QMimeData>
#include <QObject>
#include <QStringList>

class KeyExportDragHandler : public Kleo::DragHandler
{
public:
    KeyExportDragHandler();
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
};
