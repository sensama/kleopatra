/*  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

namespace GpgME
{
class Key;
}

class CertificateDetailsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CertificateDetailsDialog(QWidget *parent = nullptr);
    ~CertificateDetailsDialog() override;

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    void readConfig();
    void writeConfig();
};
