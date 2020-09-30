/*  view/smartcardwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "smartcardwidget.h"
#include "smartcard/readerstatus.h"
#include "smartcard/openpgpcard.h"
#include "smartcard/netkeycard.h"
#include "smartcard/pivcard.h"
#include "view/pgpcardwidget.h"
#include "view/netkeywidget.h"
#include "view/pivcardwidget.h"

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
                                                    << QStringLiteral("NetKey v3")
                                                    << QStringLiteral("PIV");
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

        mPIVCardWidget = new PIVCardWidget(q);
        mStack->addWidget(mPIVCardWidget);

        mPlaceHolderWidget = new PlaceHolderWidget(q);
        mStack->addWidget(mPlaceHolderWidget);

        mStack->setCurrentWidget(mPlaceHolderWidget);

        connect(ReaderStatus::instance(), &ReaderStatus::cardChanged,
                q, [this] (unsigned int slot) {
                    if (slot == 0) {
                        const auto cards = ReaderStatus::instance()->getCards();
                        if (!cards.size()) {
                            setCard(std::shared_ptr<Card>(new Card()));
                        } else {
                            // No support for multiple reader / cards currently
                            setCard(cards[0]);
                        }
                    }
                });
    }

    void setCard(std::shared_ptr<Card> card)
    {
        if (card->appName() == SmartCard::OpenPGPCard::AppName) {
            mPGPCardWidget->setCard(static_cast<OpenPGPCard *> (card.get()));
            mStack->setCurrentWidget(mPGPCardWidget);
        } else if (card->appName() == SmartCard::NetKeyCard::AppName) {
            mNetKeyWidget->setCard(static_cast<NetKeyCard *> (card.get()));
            mStack->setCurrentWidget(mNetKeyWidget);
        } else if (card->appName() == SmartCard::PIVCard::AppName) {
            mPIVCardWidget->setCard(static_cast<PIVCard *> (card.get()));
            mStack->setCurrentWidget(mPIVCardWidget);
        } else {
            mStack->setCurrentWidget(mPlaceHolderWidget);
        }
    }

private:
    SmartCardWidget *const q;
    NetKeyWidget *mNetKeyWidget;
    PGPCardWidget *mPGPCardWidget;
    PIVCardWidget *mPIVCardWidget;
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
