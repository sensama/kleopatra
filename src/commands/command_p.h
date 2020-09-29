/* -*- mode: c++; c-basic-offset:4 -*-
    commands/command_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_COMMAND_P_H__
#define __KLEOPATRA_COMMANDS_COMMAND_P_H__

#include "command.h"
#include "view/keylistcontroller.h"

#include <Libkleo/KeyListModel>

#include <KMessageBox>

#include <QAbstractItemView>
#include <QPointer>
#include <QList>
#include <QModelIndex>

#include <gpgme++/key.h>

#include <algorithm>
#include <iterator>

class Kleo::Command::Private
{
    friend class ::Kleo::Command;
protected:
    Command *const q;
public:
    explicit Private(Command *qq);
    explicit Private(Command *qq, KeyListController *controller);
    explicit Private(Command *qq, QWidget *parent);
    virtual ~Private();

    QAbstractItemView *view() const
    {
        return view_;
    }
    QWidget *parentWidgetOrView() const
    {
        if (parentWidget_) {
            return parentWidget_;
        } else {
            return view_;
        }
    }
    KeyListModelInterface *model() const
    {
        return view_ ? dynamic_cast<KeyListModelInterface *>(view_->model()) : nullptr;
    }
    KeyListController *controller() const
    {
        return controller_;
    }
    QList<QModelIndex> indexes() const
    {
        QList<QModelIndex> result;
        std::copy(indexes_.begin(), indexes_.end(), std::back_inserter(result));
        return result;
    }
    GpgME::Key key() const
    {
        return keys_.empty() ? model() && !indexes_.empty() ? model()->key(indexes_.front()) : GpgME::Key::null : keys_.front();
    }
    std::vector<GpgME::Key> keys() const
    {
        return keys_.empty() ? model() ? model()->keys(indexes()) : std::vector<GpgME::Key>() : keys_;
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
        if (parentWId) {
            KMessageBox::errorWId(parentWId, text, caption, options);
        } else {
            KMessageBox::error(parentWidgetOrView(), text, caption, options);
        }
    }
    void information(const QString &text, const QString &caption = QString(), const QString &dontShowAgainName = QString(), KMessageBox::Options options = KMessageBox::Notify) const
    {
        if (parentWId) {
            KMessageBox::informationWId(parentWId, text, caption, dontShowAgainName, options);
        } else {
            KMessageBox::information(parentWidgetOrView(), text, caption, dontShowAgainName, options);
        }
    }

    void applyWindowID(QWidget *w) const
    {
        q->applyWindowID(w);
    }

private:
    bool autoDelete : 1;
    bool warnWhenRunningAtShutdown : 1;
    std::vector<GpgME::Key> keys_;
    QList<QPersistentModelIndex> indexes_;
    QPointer<QAbstractItemView> view_;
    QPointer<QWidget> parentWidget_;
    WId parentWId = 0;
    QPointer<KeyListController> controller_;
};

#endif /* __KLEOPATRA_COMMANDS_COMMAND_P_H__ */
