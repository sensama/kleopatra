/*  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <memory>
#include <vector>

namespace GpgME
{
class Error;
class Key;
class KeyListResult;
}

class CertificateDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CertificateDetailsWidget(QWidget *parent = nullptr);
    ~CertificateDetailsWidget() override;

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    class Private;
    std::unique_ptr<Private> d;

    // Windows QGpgME new style connect problem makes this necessary.
    Q_PRIVATE_SLOT(d, void keyListDone(const GpgME::KeyListResult &, const std::vector<GpgME::Key> &, const QString &, const GpgME::Error &))
};
