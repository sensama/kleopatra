/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
    view/searchbar.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

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

#include <config-kleopatra.h>

#include "searchbar.h"

#include <Libkleo/KeyFilter>
#include <Libkleo/KeyFilterManager>

#include <KLocalizedString>
#include <QLineEdit>

#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>


#include <utils/gnupg-helper.h>
#include <qgpgme/keylistjob.h>
#include <qgpgme/protocol.h>
#include <gpgme++/keylistresult.h>

using namespace Kleo;

class SearchBar::Private
{
    friend class ::Kleo::SearchBar;
    SearchBar *const q;
public:
    explicit Private(SearchBar *qq);
    ~Private();

private:
    void slotKeyFilterChanged(int idx)
    {
        Q_EMIT q->keyFilterChanged(keyFilter(idx));
    }

    std::shared_ptr<KeyFilter> keyFilter(int idx) const
    {
        const QModelIndex mi = KeyFilterManager::instance()->model()->index(idx, 0);
        return KeyFilterManager::instance()->fromModelIndex(mi);
    }

    std::shared_ptr<KeyFilter> currentKeyFilter() const
    {
        return keyFilter(combo->currentIndex());
    }

    QString currentKeyFilterID() const
    {
        if (const std::shared_ptr<KeyFilter> f = currentKeyFilter()) {
            return f->id();
        } else {
            return QString();
        }
    }

    void listNotCertifiedKeys() const
    {
        lineEdit->clear();
        combo->setCurrentIndex(combo->findData(QStringLiteral("not-validated-certificates")));
        Q_EMIT q->keyFilterChanged(keyFilter(combo->currentIndex()));
    }

    /* List all OpenPGP keys and see if we find one with a UID that is
     * not at least fully valid.  If we find one, show the certify
     * button.  */
    /* XXX: It would be nice to do this every time the user certifies
     * a key.  */
    void showOrHideCertifyButton() const
    {
        QGpgME::KeyListJob *job = QGpgME::openpgp()->keyListJob();
        connect(job, &QGpgME::KeyListJob::result, job,
                [this](const GpgME::KeyListResult&, const std::vector<GpgME::Key> &keys, const QString&, const GpgME::Error&)
                {
                    for (const auto &key: keys) {
                        if (Kleo::keyValidity(key) < GpgME::UserID::Validity::Full) {
                            certifyButton->show();
                            return;
                        }
                    }
                    certifyButton->hide();
                });
        job->start(QStringList());
    }

private:
    QLineEdit *lineEdit;
    QComboBox *combo;
    QPushButton *certifyButton;
};

SearchBar::Private::Private(SearchBar *qq)
    : q(qq)
{
    QHBoxLayout *layout = new QHBoxLayout(q);
    layout->setContentsMargins(0, 0, 0, 0);
    lineEdit = new QLineEdit(q);
    lineEdit->setClearButtonEnabled(true);
    lineEdit->setPlaceholderText(i18n("Search..."));
    layout->addWidget(lineEdit, /*stretch=*/1);
    combo = new QComboBox(q);
    layout->addWidget(combo);
    certifyButton = new QPushButton(q);
    certifyButton->setIcon(QIcon::fromTheme(QStringLiteral("security-medium")));
    certifyButton->setToolTip(i18n("Some certificates are not yet certified. "
                                   "Click here to see a list of these certificates."
                                   "<br/><br/>"
                                   "Certification is required to make sure that the certificates "
                                   "actually belong to the identity they claim to belong to."));
    certifyButton->hide();
    layout->addWidget(certifyButton);
    showOrHideCertifyButton();

    combo->setModel(KeyFilterManager::instance()->model());

    KDAB_SET_OBJECT_NAME(layout);
    KDAB_SET_OBJECT_NAME(lineEdit);
    KDAB_SET_OBJECT_NAME(combo);
    KDAB_SET_OBJECT_NAME(certifyButton);

    connect(lineEdit, &QLineEdit::textChanged, q, &SearchBar::stringFilterChanged);
    connect(combo, SIGNAL(currentIndexChanged(int)), q, SLOT(slotKeyFilterChanged(int)));
    connect(certifyButton, SIGNAL(clicked()), q, SLOT(listNotCertifiedKeys()));
}

SearchBar::Private::~Private() {}

SearchBar::SearchBar(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f), d(new Private(this))
{

}

SearchBar::~SearchBar() {}

void SearchBar::updateClickMessage(const QString &shortcutStr)
{
    d->lineEdit->setPlaceholderText(i18n("Search...<%1>", shortcutStr));
}

// slot
void SearchBar::setStringFilter(const QString &filter)
{
    d->lineEdit->setText(filter);
}

// slot
void SearchBar::setKeyFilter(const std::shared_ptr<KeyFilter> &kf)
{
    const QModelIndex idx = KeyFilterManager::instance()->toModelIndex(kf);
    if (idx.isValid()) {
        d->combo->setCurrentIndex(idx.row());
    } else {
        d->combo->setCurrentIndex(0);
    }
}

// slot
void SearchBar::setChangeStringFilterEnabled(bool on)
{
    d->lineEdit->setEnabled(on);
}

// slot
void SearchBar::setChangeKeyFilterEnabled(bool on)
{
    d->combo->setEnabled(on);
}

QLineEdit *SearchBar::lineEdit() const
{
    return d->lineEdit;
}

#include "moc_searchbar.cpp"

