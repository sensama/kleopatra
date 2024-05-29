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

class RevokersWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RevokersWidget(QWidget *parent = nullptr);
    ~RevokersWidget() override;

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    class Private;
    const std::unique_ptr<Private> d;
};
