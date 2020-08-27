/*  commands/cardcommand_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_CARDCOMMAND_P_H__
#define __KLEOPATRA_COMMANDS_CARDCOMMAND_P_H__

#include "cardcommand.h"

#include <QPointer>

#include <KMessageBox>

class Kleo::CardCommand::Private
{
    friend class ::Kleo::CardCommand;
protected:
    CardCommand *const q;
public:
    explicit Private(CardCommand *qq, const std::string &serialNumber, QWidget *parent);
    virtual ~Private();

    std::string serialNumber() const
    {
        return serialNumber_;
    }

    QWidget *parentWidget() const
    {
        return parentWidget_;
    }

    void finished()
    {
        Q_EMIT q->finished();
        if (autoDelete) {
            q->deleteLater();
        }
    }

    void canceled()
    {
        Q_EMIT q->canceled();
        finished();
    }

    void error(const QString &text, const QString &caption = QString(), KMessageBox::Options options = KMessageBox::Notify) const
    {
        KMessageBox::error(parentWidget(), text, caption, options);
    }

    void information(const QString &text, const QString &caption = QString(), const QString &dontShowAgainName = QString(), KMessageBox::Options options = KMessageBox::Notify) const
    {
        KMessageBox::information(parentWidget(), text, caption, dontShowAgainName, options);
    }

private:
    bool autoDelete : 1;
    std::string serialNumber_;
    QPointer<QWidget> parentWidget_;
};

#endif /* __KLEOPATRA_COMMANDS_CARDCOMMAND_P_H__ */
