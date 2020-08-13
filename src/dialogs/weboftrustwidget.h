/*  SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KLEO_WEBOFTRUSTWIDGET_H
#define KLEO_WEBOFTRUSTWIDGET_H

#include <QWidget>

namespace GpgME {
class Key;
class KeyListResult;
}

namespace Kleo {

class WebOfTrustWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WebOfTrustWidget(QWidget *parent = nullptr);
    ~WebOfTrustWidget();

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private Q_SLOTS:
    void signatureListingNextKey(const GpgME::Key &key);
    void signatureListingDone(const GpgME::KeyListResult &result);

private:
    class Private;
    const QScopedPointer<Private> d;
};
} // namespace Kleo
#endif

