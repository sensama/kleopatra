/*
    view/htmllabel.h

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

class HtmlLabel : public QLabel
{
    Q_OBJECT
public:
    explicit HtmlLabel(QWidget *parent = nullptr);
    explicit HtmlLabel(const QString &html, QWidget *parent = nullptr);
    ~HtmlLabel() override;

    void setHtml(const QString &html);

    void setLinkColor(const QColor &color);

protected:
    void focusInEvent(QFocusEvent *ev) override;
    bool focusNextPrevChild(bool next) override;

private:
    using QLabel::setText;

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
