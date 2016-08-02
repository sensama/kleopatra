/*  crypto/gui/certificatelineedit.cpp

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

#include "certificatelineedit.h"

#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QCompleter>
#include <QFontMetrics>
#include <QSortFilterProxyModel>
#include <QLabel>
#include <QPushButton>
#include <QAction>
#include <QSignalBlocker>

#include "kleopatra_debug.h"

#include "dialogs/certificateselectiondialog.h"
#include "commands/detailscommand.h"

#include <Libkleo/KeyFilter>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeyListSortFilterProxyModel>
#include <Libkleo/Formatting>

#include <KLocalizedString>
#include <KIconLoader>

#include <gpgme++/key.h>

using namespace Kleo;
using namespace Kleo::Dialogs;
using namespace GpgME;

Q_DECLARE_METATYPE(Key);

#define MINIMUM_WIDTH_STR "Short Short<LongLong@MiddleDomain.co.uk> (12345678 - OpenPGP)"

CertificateLineEdit::CertificateLineEdit(AbstractKeyListModel *model,
                                         QWidget *parent,
                                         KeyFilter *filter)
    : QWidget(parent),
      mEdit(new QLineEdit()),
      mFilterModel(new KeyListSortFilterProxyModel(this)),
      mFilter(boost::shared_ptr<KeyFilter>(filter)),
      mEditStarted(false),
      mEditFinished(false),
      mLineAction(new QAction(Q_NULLPTR))
{
    auto *hLay = new QHBoxLayout;
    mEdit->setPlaceholderText(i18n("Please enter a name or email address..."));
    mEdit->setClearButtonEnabled(true);
    hLay->addWidget(mEdit, -1);
    mEdit->addAction(mLineAction, QLineEdit::LeadingPosition);

    QFontMetrics fm(font());
    mEdit->setMinimumWidth(fm.width(QStringLiteral(MINIMUM_WIDTH_STR)));

    auto *completer = new QCompleter(this);
    auto *completeFilterModel = new KeyListSortFilterProxyModel(completer);
    completeFilterModel->setKeyFilter(mFilter);
    completeFilterModel->setSourceModel(model);
    completer->setModel(completeFilterModel);
    completer->setCompletionColumn(KeyListModelInterface::Summary);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    mEdit->setCompleter(completer);
    mFilterModel->setSourceModel(model);
    mFilterModel->setFilterKeyColumn(KeyListModelInterface::Summary);
    if (filter) {
        mFilterModel->setKeyFilter(mFilter);
    }

    connect(model, &QAbstractItemModel::dataChanged,
            this, &CertificateLineEdit::updateKey);
    connect(mEdit, &QLineEdit::editingFinished,
            this, &CertificateLineEdit::updateKey);
    connect(mEdit, &QLineEdit::textChanged,
            this, &CertificateLineEdit::editChanged);
    connect(mLineAction, &QAction::triggered,
            this, &CertificateLineEdit::dialogRequested);

    setLayout(hLay);
    updateKey();

    /* Take ownership of the model to prevent double deletion when the
     * filter models are deleted */
    model->setParent(parent ? parent : this);
}

void CertificateLineEdit::editChanged()
{
    updateKey();
    if (!mEditStarted) {
        Q_EMIT editingStarted();
        mEditStarted = true;
    }
    mEditFinished = false;
}

void CertificateLineEdit::updateKey()
{
    const auto mailText = mEdit->text();
    auto newKey = Key();
    if (mailText.isEmpty()) {
        Q_EMIT wantsRemoval(this);
        mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("question")));
        mLineAction->setToolTip(i18n("Please select a certificate."));
    } else {
        mFilterModel->setFilterFixedString(mailText);
        if (mFilterModel->rowCount() > 1) {
            if (mEditFinished) {
                mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("question")).pixmap(KIconLoader::SizeSmallMedium));
                mLineAction->setToolTip(i18n("Multiple certificates"));
            }
        } else if (mFilterModel->rowCount() == 1) {
            newKey = mFilterModel->data(mFilterModel->index(0, 0), KeyListModelInterface::KeyRole).value<Key>();
            mLineAction->setToolTip(Formatting::validity(newKey.userID(0)) +
                                    QStringLiteral("<br/>Click here for details."));
            /* FIXME: This needs to be solved by a multiple UID supporting model */
            mLineAction->setIcon(Formatting::iconForUid(newKey.userID(0)));
        } else {
            mLineAction->setIcon(QIcon::fromTheme(QStringLiteral("emblem-error")));
            mLineAction->setToolTip(i18n("No matching certificates found.<br/>Click here to import a certificate."));
        }
    }
    mKey = newKey;

    if (mKey.isNull()) {
        mEdit->setToolTip(QString());
    } else {
        mEdit->setToolTip(Formatting::toolTip(newKey, Formatting::ToolTipOption::AllOptions));
    }

    Q_EMIT keyChanged();
}

Key CertificateLineEdit::key() const
{
    return mKey;
}

void CertificateLineEdit::dialogRequested()
{
    if (!mKey.isNull()) {
        auto cmd = new Commands::DetailsCommand(mKey, Q_NULLPTR);
        cmd->start();
        return;
    }

    CertificateSelectionDialog *const dlg = new CertificateSelectionDialog(this);

    dlg->setKeyFilter(mFilter);

    if (dlg->exec()) {
        const std::vector<Key> keys = dlg->selectedCertificates();
        if (!keys.size()) {
            return;
        }
        for (unsigned int i = 0; i < keys.size(); i++) {
            if (!i) {
                setKey(keys[i]);
            } else {
                Q_EMIT addRequested(keys[i]);
            }
        }
    }
    updateKey();
}

void CertificateLineEdit::setKey(const Key &k)
{
    QSignalBlocker blocky(this);
    qCDebug(KLEOPATRA_LOG) << "Setting Key. " << Formatting::summaryLine(k);
    mEdit->setText(Formatting::summaryLine(k));
}

bool CertificateLineEdit::isEmpty() const
{
    return mEdit->text().isEmpty();
}
