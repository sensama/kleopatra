/*  SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>
#include <QDialog>

namespace GpgME {
class Key;
}

namespace Kleo {
class WebOfTrustWidget;

class WebOfTrustDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WebOfTrustDialog(QWidget *parent = nullptr);
    ~WebOfTrustDialog();

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    WebOfTrustWidget *mWidget;
};

} // namespace Kleo
