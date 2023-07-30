/*  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QWidget>

namespace GpgME
{
class Key;
}

class SubKeysWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SubKeysWidget(QWidget *parent = nullptr);
    ~SubKeysWidget() override;

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    class Private;
    const QScopedPointer<Private> d;
};

class SubKeysDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SubKeysDialog(QWidget *parent = nullptr);
    ~SubKeysDialog() override;

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    void readConfig();
    void writeConfig();
};
