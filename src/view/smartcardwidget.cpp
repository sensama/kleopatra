/*  view/smartcardwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2017 Intevation GmbH

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
#include "view/pgpcardwidget.h"

#include "kleopatra_debug.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <KMessageBox>

using namespace Kleo;
using namespace Kleo::SmartCard;

namespace {
class PlaceHolderWidget: public QWidget
{
    Q_OBJECT
public:
    PlaceHolderWidget()
    {
        auto lay = new QVBoxLayout;
        lay->addStretch(-1);

        const QStringList supported = QStringList() << QStringLiteral("OpenPGP v2.0")
                                                    << QStringLiteral("OpenPGP v2.1")
                                                    << QStringLiteral("Netkey v2")
                                                    << QStringLiteral("NetKey v3");
        lay->addWidget(new QLabel(QStringLiteral("\t\t<h3>") +
                                  i18n("Please insert a compatible smartcard.") + QStringLiteral("</h3>")));
        lay->addSpacing(10);
        lay->addWidget(new QLabel(QStringLiteral("\t\t") +
                       i18n("Kleopatra currently supports the following card types:") +
                            QStringLiteral("<ul><li>") + supported.join(QStringLiteral("</li><li>")) +
                            QStringLiteral("</li></ul>")));
        lay->addStretch(-1);

        auto hLay = new QHBoxLayout;
        hLay->addStretch(-1);
        hLay->addLayout(lay);
        hLay->addStretch(-1);
        setLayout(hLay);
    }
};
} // namespace

class SmartCardWidget::Private
{
public:
    Private(SmartCardWidget *qq) : q(qq)
    {
        QPushButton *backBtn = new QPushButton(QIcon::fromTheme("arrow-left"), i18n("Back"));
        QHBoxLayout *backH = new QHBoxLayout;
        backH->addWidget(backBtn);
        backH->addWidget(new QLabel(QStringLiteral("<h2>") + i18n("Smartcard Management") +
                                    QStringLiteral("</h2>")));
        backH->addStretch(-1);

        QVBoxLayout *vLay = new QVBoxLayout;

        connect(backBtn, &QPushButton::clicked, q, [this] () {Q_EMIT (q->backRequested());});

        vLay->addLayout(backH);

        mPGPCardWidget = new PGPCardWidget;
        mPGPCardWidget->setVisible(false);
        vLay->addWidget(mPGPCardWidget);

        mPlaceHolderWidget = new PlaceHolderWidget;
        vLay->addWidget(mPlaceHolderWidget);
        vLay->addStretch(-1);

        q->setLayout(vLay);

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
            mPGPCardWidget->setVisible(true);
            mPlaceHolderWidget->setVisible(false);
        } else if (card->appType() == Card::NksApplication) {
            // TODO
        } else {
            mPlaceHolderWidget->setVisible(true);
            mPGPCardWidget->setVisible(false);
        }
    }

private:
    SmartCardWidget *q;
    // NetkeyWidget *mNetkey;
    PGPCardWidget *mPGPCardWidget;
    PlaceHolderWidget *mPlaceHolderWidget;
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
