/*  commands/listreaderscommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "listreaderscommand.h"

#include "command_p.h"

#include <Libkleo/SCDaemon>

#include <KLocalizedString>
#include <KMessageBox>

using namespace Kleo;
using namespace Kleo::Commands;

class ListReadersCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::ListReadersCommand;
    ListReadersCommand *q_func() const
    {
        return static_cast<ListReadersCommand *>(q);
    }
public:
    explicit Private(ListReadersCommand *qq, QWidget *parent);
    ~Private() override;

private:
    void start();
};

ListReadersCommand::Private *ListReadersCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const ListReadersCommand::Private *ListReadersCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

ListReadersCommand::Private::Private(ListReadersCommand *qq, QWidget *parent)
    : Command::Private(qq, parent)
{
}

ListReadersCommand::Private::~Private() = default;

void ListReadersCommand::Private::start()
{
    GpgME::Error err;
    const auto readers = SCDaemon::getReaders(err);
    QString message;
    if (err) {
        message = i18nc("@info", "Reading the list of readers failed:") + QLatin1Char{'\n'} +
                  QString::fromUtf8(err.asString()).toHtmlEscaped();
    } else if (readers.empty()) {
        message = i18nc("@info", "Available smartcard readers:") + QLatin1String{"<p>"} +
                  i18nc("@info No smartcard readers have been found", "<em>None</em>") + QLatin1String{"</p>"};
    } else {
        QStringList l;
        std::transform(std::cbegin(readers), std::cend(readers),
                       std::back_inserter(l),
                       [](const auto &s) { return QString::fromStdString(s).toHtmlEscaped(); });
        message = i18nc("@info", "Available smartcard readers:") +
                  QLatin1String{"<ul><li>"} +
                  l.join(QLatin1String{"</li><li>"}) +
                  QLatin1String{"</li></ul>"};
    }
    KMessageBox::information(parentWidgetOrView(),
                             QLatin1String{"<html>"} + message + QLatin1String{"</html>"},
                             i18nc("@title", "Smartcard Readers"));
    finished();
}

ListReadersCommand::ListReadersCommand(QWidget *parent)
    : Command(new Private(this, parent))
{
}

ListReadersCommand::~ListReadersCommand() = default;

void ListReadersCommand::doStart()
{
    d->start();
}

void ListReadersCommand::doCancel()
{
}

#undef d
#undef q
