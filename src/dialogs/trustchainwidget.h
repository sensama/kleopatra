/*  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KLEO_TRUSTCHAINWIDGET_H
#define KLEO_TRUSTCHAINWIDGET_H

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
    ~TrustChainWidget();

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
    ~TrustChainDialog();

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;
};


#endif
