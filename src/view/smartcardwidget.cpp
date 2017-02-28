/*  dialogs/smartcardwidget.cpp

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

#include "kleopatra_debug.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QInputDialog>
#include <QLineEdit>

#include <Libkleo/KeyCache>
#include <Libkleo/Formatting>

#include <KLocalizedString>
#include <KMessageBox>

using namespace Kleo;
using namespace Kleo::SmartCard;

namespace {
class PGPWidget: public QWidget
{
    Q_OBJECT
public:
    PGPWidget():
        mSerialNumber(new QLabel),
        mCardHolderLabel(new QLabel),
        mVersionLabel(new QLabel),
        mSigningKey(new QLabel),
        mEncryptionKey(new QLabel),
        mAuthKey(new QLabel),
        mCardIsEmpty(false)
    {
        auto grid = new QGridLayout;
        setLayout(grid);
        int row = 0;

        // Set up the scroll are
        auto area = new QScrollArea;
        area->setFrameShape(QFrame::NoFrame);
        area->setWidgetResizable(true);
        auto areaWidget = new QWidget;
        areaWidget->setLayout(grid);
        area->setWidget(areaWidget);
        auto myLayout = new QVBoxLayout;
        myLayout->addWidget(area);
        setLayout(myLayout);

        grid->addWidget(mVersionLabel, row++, 0, 1, 2);
        mVersionLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
        grid->addWidget(new QLabel(i18n("Serial number:")), row, 0);

        grid->addWidget(mSerialNumber, row++, 1);
        mSerialNumber->setTextInteractionFlags(Qt::TextBrowserInteraction);
        grid->addWidget(new QLabel(i18nc("The owner of a smartcard. GnuPG refers to this as cardholder.",
                        "Cardholder:")), row, 0);

        grid->addWidget(mCardHolderLabel, row, 1);
        mCardHolderLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
        auto nameButtton = new QPushButton;
        nameButtton->setIcon(QIcon::fromTheme("cell_edit"));
        nameButtton->setToolTip(i18n("Change"));
        grid->addWidget(nameButtton, row++, 2);
        connect(nameButtton, &QPushButton::clicked, this, &PGPWidget::changeNameRequested);

        auto line1 = new QFrame();
        line1->setFrameShape(QFrame::HLine);
        grid->addWidget(line1, row++, 0, 1, 4);
        grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(i18n("Keys:"))), row++, 0);

        grid->addWidget(new QLabel(i18n("Signature:")), row, 0);
        grid->addWidget(mSigningKey, row++, 1);
        mSigningKey->setTextInteractionFlags(Qt::TextBrowserInteraction);

        grid->addWidget(new QLabel(i18n("Encryption:")), row, 0);
        grid->addWidget(mEncryptionKey, row++, 1);
        mEncryptionKey->setTextInteractionFlags(Qt::TextBrowserInteraction);

        grid->addWidget(new QLabel(i18n("Authentication:")), row, 0);
        grid->addWidget(mAuthKey, row++, 1);
        mAuthKey->setTextInteractionFlags(Qt::TextBrowserInteraction);

        auto line2 = new QFrame();
        line2->setFrameShape(QFrame::HLine);
        grid->addWidget(line2, row++, 0, 1, 4);
        grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(i18n("Actions:"))), row++, 0);

        auto actionLayout = new QHBoxLayout;
        auto generateButton = new QPushButton(i18n("Generate new Keys"));
        // TODO implement action
        generateButton->setEnabled(false);
        generateButton->setToolTip(i18n("Create a new primary key and generate subkeys on the card."));
        actionLayout->addWidget(generateButton);
        connect(generateButton, &QPushButton::clicked, this, &PGPWidget::genkeyRequested);

        auto pinButtton = new QPushButton(i18n("Change PIN"));
        pinButtton->setToolTip(i18n("Change the PIN required to unblock the smartcard."));
        actionLayout->addWidget(pinButtton);
        connect(pinButtton, &QPushButton::clicked, this, [this] () {doChangePin(1);});

        auto pukButton = new QPushButton(i18n("Change Admin PIN"));
        pukButton->setToolTip(i18n("Change the PIN required to unlock the smartcard."));
        actionLayout->addWidget(pukButton);
        connect(pukButton, &QPushButton::clicked, this,  [this] () {doChangePin(3);});

        auto resetCodeButton = new QPushButton(i18n("Change Reset Code"));
        pukButton->setToolTip(i18n("Change the PIN required to reset the smartcard to an empty state."));
        actionLayout->addWidget(resetCodeButton);
        connect(resetCodeButton, &QPushButton::clicked, this, [this] () {doChangePin(2);});

        actionLayout->addStretch(-1);
        grid->addLayout(actionLayout, row++, 0, 1, 4);

        grid->setColumnStretch(4, -1);
    }

    void setCard(const OpenPGPCard* card)
    {
        mVersionLabel->setText(i18nc("First placeholder is manufacturer, second placeholder is a version number",
                                     "%1 OpenPGP v%2 card", QString::fromStdString(card->manufacturer()),
                                     QString::fromStdString(card->cardVersion())));
        mSerialNumber->setText(QString::fromStdString(card->serialNumber()));
        mCardHolderLabel->setText(QString::fromStdString(card->cardHolder()));

        updateKey(mSigningKey, card->sigFpr());
        updateKey(mEncryptionKey, card->encFpr());
        updateKey(mAuthKey, card->authFpr());
        mCardIsEmpty = card->authFpr().empty() && card->sigFpr().empty() && card->encFpr().empty();
    }

    void doChangePin(int slot)
    {
        ReaderStatus::mutableInstance()
        ->startSimpleTransaction(QStringLiteral("SCD PASSWD %1").arg(slot).toUtf8().constData(),
                                 this, "changePinResult");
    }

public Q_SLOTS:
    void genkeyRequested()
    {
        if (mCardIsEmpty) {
//            GenKeyWidget *widget;
//            QDialog *dialog = 
        }
    }

    void changePinResult(const GpgME::Error &err)
    {
        if (err) {
            KMessageBox::error(this, i18nc("@info",
                               "PIN change failed: %1", err.asString()),
                               i18nc("@title", "Error"));
            return;
        }
        if (!err.isCanceled()) {
            KMessageBox::information(this, i18nc("@info",
                        "Code successfully changed."),
                    i18nc("@title", "Success"));
        }
    }

    void changeNameRequested()
    {
        QString text = mCardHolderLabel->text();
        while (true) {
            bool ok = false;
            text = QInputDialog::getText(this, i18n("Change cardholder"),
                                         i18n("New name:"), QLineEdit::Normal,
                                         text, &ok, Qt::WindowFlags(),
                                         Qt::ImhLatinOnly);
            if (!ok) {
                return;
            }
            // Some additional restrictions imposed by gnupg
            if (text.contains("<")) {
                KMessageBox::error(this, i18nc("@info",
                                   "The \"<\" character may not be used."),
                                   i18nc("@title", "Error"));
                continue;
            }
            if (text.contains("  ")) {
                KMessageBox::error(this, i18nc("@info",
                                   "Double spaces are not allowed"),
                                   i18nc("@title", "Error"));
                continue;
            }
            if (text.size() > 38) {
                KMessageBox::error(this, i18nc("@info",
                                   "The size of the name may not exceed 38 characters."),
                                   i18nc("@title", "Error"));
            }
            break;
        }
        auto parts = text.split(" ");
        const auto lastName = parts.takeLast();
        const auto formatted = lastName + QStringLiteral("<<") + parts.join("<");

        ReaderStatus::mutableInstance()
        ->startSimpleTransaction(QStringLiteral("SCD SETATTR DISP-NAME %1").arg(formatted).toUtf8().constData(),
                                 this, "changeNameResult");

    }

    void changeNameResult(const GpgME::Error &err)
    {
        if (err) {
            KMessageBox::error(this, i18nc("@info",
                               "Name change failed: %1", err.asString()),
                               i18nc("@title", "Error"));
            return;
        }
        if (!err.isCanceled()) {
            KMessageBox::information(this, i18nc("@info",
                        "Name successfully changed."),
                    i18nc("@title", "Success"));
            ReaderStatus::mutableInstance()->updateStatus();
        }

    }

private:
    void updateKey(QLabel *label, const std::string &fpr)
    {
        label->setText(fpr.c_str());

        if (fpr.empty()) {
            label->setText(i18n("Slot empty"));
            return;
        }

        std::vector<std::string> vec;
        std::string keyid = fpr;
        keyid.erase(0, keyid.size() - 16);
        vec.push_back(keyid);
        const auto subkeys = KeyCache::instance()->findSubkeysByKeyID(vec);
        if (subkeys.empty() || subkeys[0].isNull()) {
            label->setToolTip(i18n("Public key not found."));
            return;
        }
        QStringList toolTips;
        for (const auto &sub: subkeys) {
            // Yep you can have one subkey associated with mutliple
            // primary keys.
            toolTips << Formatting::toolTip(sub.parent(), Formatting::Validity |
                                            Formatting::StorageLocation |
                                            Formatting::ExpiryDates |
                                            Formatting::UserIDs |
                                            Formatting::Fingerprint);
        }
        label->setToolTip(toolTips.join("<br/>"));
        return;
    }
    QLabel *mSerialNumber,
           *mCardHolderLabel,
           *mVersionLabel,
           *mSigningKey,
           *mEncryptionKey,
           *mAuthKey;
    bool mCardIsEmpty;
};
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

        mPGPWidget = new PGPWidget;
        vLay->addWidget(mPGPWidget);

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
            mPGPWidget->setCard(static_cast<OpenPGPCard *> (card.get()));
            mPGPWidget->setVisible(true);
            mPlaceHolderWidget->setVisible(false);
        } else if (card->appType() == Card::NksApplication) {
            // TODO
        } else {
            mPlaceHolderWidget->setVisible(true);
            mPGPWidget->setVisible(false);
        }
    }

private:
    SmartCardWidget *q;
    // NetkeyWidget *mNetkey;
    PGPWidget *mPGPWidget;
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
