/*  commands/cardcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_COMMANDS_CARDCOMMAND_H__
#define __KLEOPATRA_COMMANDS_CARDCOMMAND_H__

#include <QObject>

class QWidget;

#include <utils/pimpl_ptr.h>

namespace Kleo
{

class CardCommand : public QObject
{
    Q_OBJECT
public:
    explicit CardCommand(const std::string &serialNumber, QWidget *parent);
    ~CardCommand() override;

   void setAutoDelete(bool on);
   bool autoDelete() const;

public Q_SLOTS:
    void start();
    void cancel();

Q_SIGNALS:
    void finished();
    void canceled();

private:
    virtual void doStart() = 0;
    virtual void doCancel();

protected:
    class Private;
    kdtools::pimpl_ptr<Private> d;
protected:
    explicit CardCommand(Private *pp);
};

} // namespace Kleo

#endif /* __KLEOPATRA_COMMANDS_CARDCOMMAND_H__ */
