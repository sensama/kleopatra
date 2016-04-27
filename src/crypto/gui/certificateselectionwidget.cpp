/*  crypto/gui/certificateselectionwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2016 Intevation GmbH

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

#include "certificateselectionwidget.h"

#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QCompleter>
#include <QFontMetrics>

#include <boost/shared_ptr.hpp>

#include "models/keylistmodel.h"
#include "models/keylistsortfilterproxymodel.h"
#include "models/keycache.h"
#include "dialogs/certificateselectiondialog.h"
#include "certificatecombobox.h"

#include <KLocalizedString>

#include <gpgme++/key.h>

using namespace Kleo;
using namespace Kleo::Dialogs;
using namespace GpgME;

Q_DECLARE_METATYPE(Key);

#define MINIMUM_WIDTH_STR "Short LongLong <LongLong@MiddleDomain.co.uk> (12345678 - OpenPGP)"
#define MINIMUM_MAIL_WIDTH_STR "Short.Short@MiddleDomain.co.uk"
CertificateSelectionWidget::CertificateSelectionWidget(QWidget *parent,
                                                       int options, bool mailbox,
                                                       const QString &defaultFpr)
    : QWidget(parent),
      mCombo(new CertificateComboBox(i18n("Please select a certificate"))),
      mMailEntry(new QLineEdit()),
      mOptions(options),
      mModel(AbstractKeyListModel::createFlatKeyListModel(this)),
      mFilterModel(new KeyListSortFilterProxyModel(this))
{
    QHBoxLayout *hLay = new QHBoxLayout;
    mMailEntry->setPlaceholderText(i18n("E-Mail"));
    mMailEntry->setVisible(mailbox);
    hLay->addWidget(mMailEntry, 1);
    hLay->addWidget(mCombo, 1);
    QFontMetrics fm(font());
    mCombo->setMinimumWidth(fm.width(QStringLiteral(MINIMUM_WIDTH_STR)));
    mCombo->setMaxVisibleItems(8); // According to VDG
    mMailEntry->setMinimumWidth(fm.width(QStringLiteral(MINIMUM_MAIL_WIDTH_STR)));

    QCompleter *completer = new QCompleter(this);
    completer->setModel(mModel);
    completer->setCompletionRole(Qt::EditRole);
    completer->setCompletionColumn(KeyListModelInterface::PrettyEMail);
    mMailEntry->setCompleter(completer);

    mFilterModel->setSourceModel(mModel);
    mCombo->setModel(mFilterModel);
    mCombo->setModelColumn(KeyListModelInterface::Summary);

    connect(KeyCache::instance().get(), &KeyCache::keysMayHaveChanged,
            this, &CertificateSelectionWidget::keysMayHaveChanged);
    connect(mMailEntry, &QLineEdit::textChanged,
            this, &CertificateSelectionWidget::mailEntryChanged);
    connect(mCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &CertificateSelectionWidget::keyChanged);

    keysMayHaveChanged();

    if (mailbox) {
        // When there is no mail entry shown we just show our stuff.
        mCombo->setCurrentIndex(-1);
    }

    setLayout(hLay);
}

void CertificateSelectionWidget::keysMayHaveChanged()
{
    std::vector<Key> keys = (mOptions & CertificateSelectionDialog::SecretKeys) ? KeyCache::instance()->secretKeys()
                                                                                : KeyCache::instance()->keys();
    CertificateSelectionDialog::filterAllowedKeys(keys, mOptions);
    bool wasUnselected = mCombo->currentIndex() == -1;
    mModel->setKeys(keys);
    mFilterModel->sort(KeyListModelInterface::Summary);
    if (wasUnselected && mMailEntry->isVisible() && mMailEntry->text().isEmpty()) {
        mCombo->setCurrentIndex(-1);
    }
}

void CertificateSelectionWidget::mailEntryChanged()
{
    const QString mailText = mMailEntry->text();
    if (mailText.isEmpty()) {
        mCombo->setCurrentIndex(-1);
        return;
    }
    mFilterModel->setFilterFixedString(mailText);
    if (mFilterModel->rowCount()) {
        mCombo->setModel(mFilterModel);
        if (mCombo->currentIndex() == -1) {
            mCombo->setCurrentIndex(0);
        }
    } else {
        mCombo->setInitialText(i18n("(no matching certificates found)"));
        mCombo->setCurrentIndex(-1);
    }
}

Key CertificateSelectionWidget::key() const
{
    int idx = mCombo->currentIndex();
    if (idx == -1) {
        return Key();
    }
    return mCombo->currentData(KeyListModelInterface::KeyRole).value<Key>();
}
