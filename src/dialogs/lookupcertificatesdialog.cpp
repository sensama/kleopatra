/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/lookupcertificatesdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB

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

#include "lookupcertificatesdialog.h"

#include "ui_lookupcertificatesdialog.h"

#include <Libkleo/KeyListModel>
#include <KConfigGroup>
#include <gpgme++/key.h>

#include <KLocalizedString>

#include <QPushButton>
#include <QTreeView>

#include <KSharedConfig>

using namespace Kleo;
using namespace Kleo::Dialogs;
using namespace GpgME;

static const int minimalSearchTextLength = 2;

class LookupCertificatesDialog::Private
{
    friend class ::Kleo::Dialogs::LookupCertificatesDialog;
    LookupCertificatesDialog *const q;
public:
    explicit Private(LookupCertificatesDialog *qq);
    ~Private();

private:
    void slotSelectionChanged()
    {
        enableDisableWidgets();
    }
    void slotSearchTextChanged()
    {
        enableDisableWidgets();
    }
    void slotSearchClicked()
    {
        Q_EMIT q->searchTextChanged(ui.findED->text());
    }
    void slotDetailsClicked()
    {
        Q_ASSERT(q->selectedCertificates().size() == 1);
        Q_EMIT q->detailsRequested(q->selectedCertificates().front());
    }
    void slotSaveAsClicked()
    {
        Q_EMIT q->saveAsRequested(q->selectedCertificates());
    }

    void readConfig();
    void writeConfig();
    void enableDisableWidgets();

    QString searchText() const
    {
        return ui.findED->text().trimmed();
    }

    std::vector<Key> selectedCertificates() const
    {
        const QAbstractItemView *const view = ui.resultTV->view();
        if (!view) {
            return std::vector<Key>();
        }
        const auto *const model = dynamic_cast<KeyListModelInterface*>(view->model());
        Q_ASSERT(model);
        const QItemSelectionModel *const sm = view->selectionModel();
        Q_ASSERT(sm);
        return model->keys(sm->selectedRows());
    }

    int numSelectedCertificates() const
    {
        return ui.resultTV->selectedKeys().size();
    }
private:
    bool passive;

    struct Ui : Ui_LookupCertificatesDialog {

        explicit Ui(LookupCertificatesDialog *q)
            : Ui_LookupCertificatesDialog()
        {
            setupUi(q);

            saveAsPB->hide(); // ### not yet implemented in LookupCertificatesCommand

            findED->setClearButtonEnabled(true);

            resultTV->setFlatModel(AbstractKeyListModel::createFlatKeyListModel(q));
            resultTV->setHierarchicalView(false);

            importPB()->setText(i18n("Import"));
            importPB()->setEnabled(false);

            connect(resultTV->view(), SIGNAL(doubleClicked(QModelIndex)),
                    importPB(), SLOT(animateClick()));

            findED->setFocus();

            connect(selectAllPB, &QPushButton::clicked,
                    resultTV->view(), &QTreeView::selectAll);
            connect(deselectAllPB, &QPushButton::clicked,
                    resultTV->view(), &QTreeView::clearSelection);
        }

        QPushButton *importPB() const
        {
            return buttonBox->button(QDialogButtonBox::Save);
        }
        QPushButton *closePB()  const
        {
            return buttonBox->button(QDialogButtonBox::Close);
        }
    } ui;
};

LookupCertificatesDialog::Private::Private(LookupCertificatesDialog *qq)
    : q(qq),
      passive(false),
      ui(q)
{
    connect(ui.resultTV->view()->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            q, SLOT(slotSelectionChanged()));
}

LookupCertificatesDialog::Private::~Private() {}

void LookupCertificatesDialog::Private::readConfig()
{
    KConfigGroup dialog(KSharedConfig::openConfig(), "LookupCertificatesDialog");
    const QSize size = dialog.readEntry("Size", QSize(600, 400));
    if (size.isValid()) {
        q->resize(size);
    }
}

void LookupCertificatesDialog::Private::writeConfig()
{
    KConfigGroup dialog(KSharedConfig::openConfig(), "LookupCertificatesDialog");
    dialog.writeEntry("Size", q->size());
    dialog.sync();
}

LookupCertificatesDialog::LookupCertificatesDialog(QWidget *p, Qt::WindowFlags f)
    : QDialog(p, f), d(new Private(this))
{
    d->ui.findPB->setEnabled(false);
    d->readConfig();
}

LookupCertificatesDialog::~LookupCertificatesDialog()
{
    d->writeConfig();
}

void LookupCertificatesDialog::setCertificates(const std::vector<Key> &certs)
{
    d->ui.resultTV->view()->setFocus();
    d->ui.resultTV->setKeys(certs);
}

std::vector<Key> LookupCertificatesDialog::selectedCertificates() const
{
    return d->selectedCertificates();
}

void LookupCertificatesDialog::setPassive(bool on)
{
    if (d->passive == on) {
        return;
    }
    d->passive = on;
    d->enableDisableWidgets();
}

bool LookupCertificatesDialog::isPassive() const
{
    return d->passive;
}

void LookupCertificatesDialog::setSearchText(const QString &text)
{
    d->ui.findED->setText(text);
}

QString LookupCertificatesDialog::searchText() const
{
    return d->ui.findED->text();
}

void LookupCertificatesDialog::accept()
{
    Q_ASSERT(!selectedCertificates().empty());
    Q_EMIT importRequested(selectedCertificates());
    QDialog::accept();
}

void LookupCertificatesDialog::Private::enableDisableWidgets()
{
    // enable/disable everything except 'close', based on passive:
    Q_FOREACH (QObject *const o, q->children())
        if (QWidget *const w = qobject_cast<QWidget *>(o)) {
            w->setDisabled(passive && w != ui.closePB() && w != ui.buttonBox);
        }

    if (passive) {
        return;
    }

    ui.findPB->setEnabled(searchText().length() > minimalSearchTextLength);

    const int n = q->selectedCertificates().size();

    ui.detailsPB->setEnabled(n == 1);
    ui.saveAsPB->setEnabled(n == 1);
    ui.importPB()->setEnabled(n != 0);
    ui.importPB()->setDefault(false);   // otherwise Import becomes default button if enabled and return triggers both a search and accept()
}

#include "moc_lookupcertificatesdialog.cpp"

