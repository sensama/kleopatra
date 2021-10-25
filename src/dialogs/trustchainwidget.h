/*  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>
#include <QDialog>

namespace GpgME {
class Key;
}

class TrustChainWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrustChainWidget(QWidget *parent = nullptr);
    ~TrustChainWidget() override;

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    class Private;
    const QScopedPointer<Private> d;
};

class TrustChainDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TrustChainDialog(QWidget *parent = nullptr);
    ~TrustChainDialog() override;

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;
};


