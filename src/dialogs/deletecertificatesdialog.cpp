/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/deletecertificatesdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2009 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "deletecertificatesdialog.h"

#include <utils/accessibility.h>
#include <view/keytreeview.h>

#include <Libkleo/KeyListModel>
#include <Libkleo/Stl_Util>

#include "kleopatra_debug.h"
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>
#include <KStandardGuiItem>

#include <QCursor>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWhatsThis>

#include <gpgme++/key.h>

using namespace Kleo;
using namespace Kleo::Dialogs;
using namespace GpgME;

class DeleteCertificatesDialog::Private
{
    friend class ::Kleo::Dialogs::DeleteCertificatesDialog;
    DeleteCertificatesDialog *const q;

public:
    explicit Private(DeleteCertificatesDialog *qq)
        : q(qq)
        , ui(q)
    {
    }

    void slotWhatsThisRequested()
    {
        qCDebug(KLEOPATRA_LOG);
        if (QWidget *const widget = qobject_cast<QWidget *>(q->sender()))
            if (!widget->whatsThis().isEmpty()) {
                showToolTip(QCursor::pos(), widget->whatsThis(), widget);
            }
    }

    void readConfig()
    {
        KConfigGroup dialog(KSharedConfig::openStateConfig(), QStringLiteral("DeleteCertificatesDialog"));
        ui.selectedKTV.restoreLayout(dialog);
        ui.unselectedKTV.restoreLayout(dialog);
        const QSize size = dialog.readEntry("Size", QSize(600, 400));
        if (size.isValid()) {
            q->resize(size);
        }
    }

    void writeConfig()
    {
        KConfigGroup dialog(KSharedConfig::openStateConfig(), QStringLiteral("DeleteCertificatesDialog"));
        dialog.writeEntry("Size", q->size());
        dialog.sync();
    }

private:
    struct UI {
        QLabel selectedLB;
        KeyTreeView selectedKTV;
        QLabel unselectedLB;
        KeyTreeView unselectedKTV;
        QDialogButtonBox buttonBox;
        QVBoxLayout vlay;

        explicit UI(DeleteCertificatesDialog *qq)
            : selectedLB(i18n("These are the certificates you have selected for deletion:"), qq)
            , selectedKTV(qq)
            , unselectedLB(i18n("These certificates will be deleted even though you did <b>not</b> "
                                "explicitly select them (<a href=\"whatsthis://\">Why?</a>):"),
                           qq)
            , unselectedKTV(qq)
            , buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel)
            , vlay(qq)
        {
            KDAB_SET_OBJECT_NAME(selectedLB);
            KDAB_SET_OBJECT_NAME(selectedKTV);
            KDAB_SET_OBJECT_NAME(unselectedLB);
            KDAB_SET_OBJECT_NAME(unselectedKTV);
            KDAB_SET_OBJECT_NAME(buttonBox);
            KDAB_SET_OBJECT_NAME(vlay);

            vlay.addWidget(&selectedLB);
            vlay.addWidget(&selectedKTV, 1);
            vlay.addWidget(&unselectedLB);
            vlay.addWidget(&unselectedKTV, 1);
            vlay.addWidget(&buttonBox);

            const QString unselectedWhatsThis = xi18nc("@info:whatsthis",
                                                       "<title>Why do you want to delete more certificates than I selected?</title>"
                                                       "<para>When you delete CA certificates (both root CAs and intermediate CAs), "
                                                       "the certificates issued by them will also be deleted.</para>"
                                                       "<para>This can be nicely seen in <application>Kleopatra</application>'s "
                                                       "hierarchical view mode: In this mode, if you delete a certificate that has "
                                                       "children, those children will also be deleted. Think of CA certificates as "
                                                       "folders containing other certificates: When you delete the folder, you "
                                                       "delete its contents, too.</para>");
            unselectedLB.setContextMenuPolicy(Qt::NoContextMenu);
            unselectedLB.setWhatsThis(unselectedWhatsThis);
            unselectedKTV.setWhatsThis(unselectedWhatsThis);

            buttonBox.button(QDialogButtonBox::Ok)->setText(i18nc("@action:button", "Delete"));

            connect(&unselectedLB, SIGNAL(linkActivated(QString)), qq, SLOT(slotWhatsThisRequested()));

            selectedKTV.setFlatModel(AbstractKeyListModel::createFlatKeyListModel(&selectedKTV));
            unselectedKTV.setFlatModel(AbstractKeyListModel::createFlatKeyListModel(&unselectedKTV));

            selectedKTV.setHierarchicalView(false);
            selectedKTV.view()->setSelectionMode(QAbstractItemView::NoSelection);
            unselectedKTV.setHierarchicalView(false);
            unselectedKTV.view()->setSelectionMode(QAbstractItemView::NoSelection);

            connect(&buttonBox, SIGNAL(accepted()), qq, SLOT(accept()));
            connect(&buttonBox, &QDialogButtonBox::rejected, qq, &QDialog::reject);
        }
    } ui;
};

DeleteCertificatesDialog::DeleteCertificatesDialog(QWidget *p)
    : QDialog(p)
    , d(new Private(this))
{
    d->readConfig();
}

DeleteCertificatesDialog::~DeleteCertificatesDialog()
{
    d->writeConfig();
}

void DeleteCertificatesDialog::setSelectedKeys(const std::vector<Key> &keys)
{
    d->ui.selectedKTV.setKeys(keys);
}

void DeleteCertificatesDialog::setUnselectedKeys(const std::vector<Key> &keys)
{
    d->ui.unselectedLB.setVisible(!keys.empty());
    d->ui.unselectedKTV.setVisible(!keys.empty());
    d->ui.unselectedKTV.setKeys(keys);
}

std::vector<Key> DeleteCertificatesDialog::keys() const
{
    const std::vector<Key> sel = d->ui.selectedKTV.keys();
    const std::vector<Key> uns = d->ui.unselectedKTV.keys();
    std::vector<Key> result;
    result.reserve(sel.size() + uns.size());
    result.insert(result.end(), sel.begin(), sel.end());
    result.insert(result.end(), uns.begin(), uns.end());
    return result;
}

void DeleteCertificatesDialog::accept()
{
    const std::vector<Key> sel = d->ui.selectedKTV.keys();
    const std::vector<Key> uns = d->ui.unselectedKTV.keys();

    const uint secret =
        std::count_if(sel.cbegin(), sel.cend(), std::mem_fn(&Key::hasSecret)) + std::count_if(uns.cbegin(), uns.cend(), std::mem_fn(&Key::hasSecret));
    const uint total = sel.size() + uns.size();

    int ret = KMessageBox::Continue;
    if (secret)
        ret = KMessageBox::warningContinueCancel(this,
                                                 secret == total ? i18np("The certificate to be deleted is your own. "
                                                                         "It contains private key material, "
                                                                         "which is needed to decrypt past communication "
                                                                         "encrypted to the certificate, and should therefore "
                                                                         "not be deleted.",

                                                                         "All of the certificates to be deleted "
                                                                         "are your own. "
                                                                         "They contain private key material, "
                                                                         "which is needed to decrypt past communication "
                                                                         "encrypted to the certificate, and should therefore "
                                                                         "not be deleted.",

                                                                         secret)
                                                                 : i18np("One of the certificates to be deleted "
                                                                         "is your own. "
                                                                         "It contains private key material, "
                                                                         "which is needed to decrypt past communication "
                                                                         "encrypted to the certificate, and should therefore "
                                                                         "not be deleted.",

                                                                         "Some of the certificates to be deleted "
                                                                         "are your own. "
                                                                         "They contain private key material, "
                                                                         "which is needed to decrypt past communication "
                                                                         "encrypted to the certificate, and should therefore "
                                                                         "not be deleted.",

                                                                         secret),
                                                 i18nc("@title:window", "Secret Key Deletion"),
                                                 KStandardGuiItem::guiItem(KStandardGuiItem::Delete),
                                                 KStandardGuiItem::cancel(),
                                                 QString(),
                                                 KMessageBox::Notify | KMessageBox::Dangerous);

    if (ret == KMessageBox::Continue) {
        QDialog::accept();
    } else {
        QDialog::reject();
    }
}

#include "moc_deletecertificatesdialog.cpp"
