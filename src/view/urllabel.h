/*
    view/urllabel.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QLabel>

#include <memory>

namespace Kleo
{

class UrlLabel : public QLabel
{
    Q_OBJECT
public:
    explicit UrlLabel(QWidget *parent = nullptr);
    ~UrlLabel() override;

    void setUrl(const QUrl &url, const QString &text = {});

    void setLinkColor(const QColor &color);

protected:
    void focusInEvent(QFocusEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    using QLabel::setText;

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
