/*  view/smartcardwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

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

#include <KLocalizedString>

#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QStackedWidget>

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
    Private(SmartCardWidget *qq);

    void cardAddedOrChanged(const std::string &serialNumber, const std::string &appName);
    void cardRemoved(const std::string &serialNumber, const std::string &appName);

private:
    template <typename C, typename W>
    void cardAddedOrChanged(const std::string &serialNumber);

private:
    SmartCardWidget *const q;
    QMap<std::pair<std::string, std::string>, QPointer<QWidget> > mCardWidgets;
    PlaceHolderWidget *mPlaceHolderWidget;
    QStackedWidget *mStack;
    QTabWidget *mTabWidget;
};

SmartCardWidget::Private::Private(SmartCardWidget *qq)
    : q(qq)
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

    mPlaceHolderWidget = new PlaceHolderWidget;
    mStack->addWidget(mPlaceHolderWidget);

    mTabWidget = new QTabWidget;
    mStack->addWidget(mTabWidget);

    mStack->setCurrentWidget(mPlaceHolderWidget);

    connect(ReaderStatus::instance(), &ReaderStatus::cardAdded,
            q, [this] (const std::string &serialNumber, const std::string &appName) { cardAddedOrChanged(serialNumber, appName); });
    connect(ReaderStatus::instance(), &ReaderStatus::cardChanged,
            q, [this] (const std::string &serialNumber, const std::string &appName) { cardAddedOrChanged(serialNumber, appName); });
    connect(ReaderStatus::instance(), &ReaderStatus::cardRemoved,
            q, [this] (const std::string &serialNumber, const std::string &appName) { cardRemoved(serialNumber, appName); });
}

void SmartCardWidget::Private::cardAddedOrChanged(const std::string &serialNumber, const std::string &appName)
{
    if (appName == SmartCard::NetKeyCard::AppName) {
        cardAddedOrChanged<NetKeyCard, NetKeyWidget>(serialNumber);
    } else if (appName == SmartCard::OpenPGPCard::AppName) {
        cardAddedOrChanged<OpenPGPCard, PGPCardWidget>(serialNumber);
    } else if (appName == SmartCard::PIVCard::AppName) {
        cardAddedOrChanged<PIVCard, PIVCardWidget>(serialNumber);
    } else {
        qCWarning(KLEOPATRA_LOG) << "SmartCardWidget::Private::cardAddedOrChanged:"
            << "App" << appName.c_str() << "is not supported";
    }
}

template <typename C, typename W>
void SmartCardWidget::Private::cardAddedOrChanged(const std::string &serialNumber)
{
    const auto card = ReaderStatus::instance()->getCard<C>(serialNumber);
    if (!card) {
        qCWarning(KLEOPATRA_LOG) << "SmartCardWidget::Private::cardAddedOrChanged:"
                                 << "New or changed card" << serialNumber.c_str() << "with app" << C::AppName.c_str() << "not found";
        return;
    }
    W *cardWidget = dynamic_cast<W *>(mCardWidgets.value({serialNumber, C::AppName}).data());
    if (!cardWidget) {
        cardWidget = new W;
        mCardWidgets.insert({serialNumber, C::AppName}, cardWidget);
        const QString cardLabel = i18nc("@title:tab serial number of smartcard - smartcard application", "%1 - %2",
                                        QString::fromStdString(serialNumber), QString::fromStdString(C::AppName));
        mTabWidget->addTab(cardWidget, cardLabel);
        if (mCardWidgets.size() == 1) {
            mStack->setCurrentWidget(mTabWidget);
        }
    }
    cardWidget->setCard(card.get());
}

void SmartCardWidget::Private::cardRemoved(const std::string &serialNumber, const std::string &appName)
{
    QWidget * cardWidget = mCardWidgets.take({serialNumber, appName});
    if (cardWidget) {
        const int index = mTabWidget->indexOf(cardWidget);
        if (index != -1) {
            mTabWidget->removeTab(index);
        }
        delete cardWidget;
    }
    if (mCardWidgets.empty()) {
        mStack->setCurrentWidget(mPlaceHolderWidget);
    }
}

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
