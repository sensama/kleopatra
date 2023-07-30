/*  SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <memory>

namespace GpgME
{
class Key;
class KeyListResult;
}

namespace Kleo
{

class WebOfTrustWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WebOfTrustWidget(QWidget *parent = nullptr);
    ~WebOfTrustWidget() override;

    QAction *detailsAction() const;
    QAction *certifyAction() const;
    QAction *revokeAction() const;

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private Q_SLOTS:
    void signatureListingNextKey(const GpgME::Key &key);
    void signatureListingDone(const GpgME::KeyListResult &result);

private:
    class Private;
    const std::unique_ptr<Private> d;
};
} // namespace Kleo
