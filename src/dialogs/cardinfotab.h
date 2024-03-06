// SPDX-FileCopyrightText: 2024 g10 Code GmbH
// SPDX-FileContributor: Tobias Fella <tobias.fella@gnupg.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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

class CardInfoTab : public QWidget
{
    Q_OBJECT

public:
    explicit CardInfoTab(QWidget *parent = nullptr);
    ~CardInfoTab() override;

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    class Private;
    const std::unique_ptr<Private> d;
};
} // namespace Kleo
