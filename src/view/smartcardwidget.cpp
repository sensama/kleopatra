/*  view/smartcardwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2017 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include "smartcardwidget.h"
#include "smartcard/readerstatus.h"
#include "smartcard/openpgpcard.h"
#include "smartcard/netkeycard.h"
#include "view/pgpcardwidget.h"
#include "view/netkeywidget.h"

#include "kleopatra_debug.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStackedWidget>

#include <KLocalizedString>

using namespace Kleo;
using namespace Kleo::SmartCard;

namespace {
class PlaceHolderWidget: public QWidget
{
    Q_OBJECT
public:
    explicit PlaceHolderWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto lay = new QVBoxLayout;
        lay->addStretch(-1);

        const QStringList supported = QStringList() << QStringLiteral("OpenPGP v2.0 - v3.3")
                                                    << QStringLiteral("Gnuk")
                                                    << QStringLiteral("NetKey v3");
        lay->addWidget(new QLabel(QStringLiteral("\t\t<h3>") +
                                  i18n("Please insert a compatible smartcard.") + QStringLiteral("</h3>"), this));
        lay->addSpacing(10);
        lay->addWidget(new QLabel(QStringLiteral("\t\t") +
                       i18n("Kleopatra currently supports the following card types:") +
                            QStringLiteral("<ul><li>") + supported.join(QLatin1String("</li><li>")) +
                            QStringLiteral("</li></ul>"), this));
        lay->addSpacing(10);
        lay->addWidget(new QLabel(i18n("Refresh the view (F5) to update the smartcard status."), this));
        lay->addStretch(-1);

        auto hLay = new QHBoxLayout(this);
        hLay->addStretch(-1);
        hLay->addLayout(lay);
        hLay->addStretch(-1);
        lay->addStretch(-1);
    }
};
} // namespace

class SmartCardWidget::Private
{
public:
    Private(SmartCardWidget *qq) : q(qq)
    {
        QPushButton *backBtn = new QPushButton(QIcon::fromTheme(QStringLiteral("arrow-left")), i18n("Back"));
        QHBoxLayout *backH = new QHBoxLayout;
        backH->addWidget(backBtn);
        backH->addWidget(new QLabel(QStringLiteral("<h2>") + i18n("Smartcard Management") +
                                    QStringLiteral("</h2>")));
        backH->addStretch(-1);

        QVBoxLayout *vLay = new QVBoxLayout(q);


        connect(backBtn, &QPushButton::clicked, q, [this] () {Q_EMIT q->backRequested();});

        vLay->addLayout(backH);

        mStack = new QStackedWidget;
        vLay->addWidget(mStack);

        mPGPCardWidget = new PGPCardWidget(q);
        mStack->addWidget(mPGPCardWidget);

        mNetKeyWidget = new NetKeyWidget(q);
        mStack->addWidget(mNetKeyWidget);

        mPlaceHolderWidget = new PlaceHolderWidget(q);
        mStack->addWidget(mPlaceHolderWidget);

        mStack->setCurrentWidget(mPlaceHolderWidget);

        connect (ReaderStatus::instance(), &ReaderStatus::cardChanged, q, [this] (unsigned int /*slot*/) {
                const auto cards = ReaderStatus::instance()->getCards();
                if (!cards.size()) {
                    setCard(std::shared_ptr<Card>(new Card()));
                } else {
                    // No support for multiple reader / cards currently
                    setCard(cards[0]);
                }
            });
    }

    void setCard(std::shared_ptr<Card> card)
    {
        if (card->appType() == Card::OpenPGPApplication) {
            mPGPCardWidget->setCard(static_cast<OpenPGPCard *> (card.get()));
            mStack->setCurrentWidget(mPGPCardWidget);
        } else if (card->appType() == Card::NksApplication) {
            mNetKeyWidget->setCard(static_cast<NetKeyCard *> (card.get()));
            mStack->setCurrentWidget(mNetKeyWidget);
        } else {
            mStack->setCurrentWidget(mPlaceHolderWidget);
        }
    }

private:
    SmartCardWidget *const q;
    NetKeyWidget *mNetKeyWidget;
    PGPCardWidget *mPGPCardWidget;
    PlaceHolderWidget *mPlaceHolderWidget;
    QStackedWidget *mStack;
};

SmartCardWidget::SmartCardWidget(QWidget *parent):
    QWidget(parent),
    d(new Private(this))
{
}

void SmartCardWidget::reload()
{
    ReaderStatus::mutableInstance()->updateStatus();
}

#include "smartcardwidget.moc"
