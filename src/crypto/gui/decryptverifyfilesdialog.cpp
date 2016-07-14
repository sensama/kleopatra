/* crypto/gui/decryptverifyfilesdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB
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

#include "decryptverifyfilesdialog.h"

#include "kleopatra_debug.h"

#include "crypto/taskcollection.h"
#include "crypto/decryptverifytask.h"
#include "crypto/gui/resultpage.h"
#include "crypto/gui/resultlistwidget.h"

#include <Libkleo/FileNameRequester>

#include <QVBoxLayout>
#include <QProgressBar>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>

#include <vector>
#include <boost/shared_ptr.hpp>

#include <KLocalizedString>
#include <KMessageBox>

using namespace Kleo;
using namespace Kleo::Crypto;
using namespace Kleo::Crypto::Gui;

DecryptVerifyFilesDialog::DecryptVerifyFilesDialog(const boost::shared_ptr<TaskCollection> &coll, QWidget *parent)
    : QDialog(parent), m_tasks(coll), m_buttonBox(new QDialogButtonBox)
{
    auto vLay = new QVBoxLayout(this);
    auto labels = new QWidget;
    auto outputLayout = new QHBoxLayout;

    m_outputLocationFNR = new FileNameRequester;
    auto outLabel = new QLabel(i18n("&Output folder:"));
    outLabel->setBuddy(m_outputLocationFNR);
    outputLayout->addWidget(outLabel);
    outputLayout->addWidget(m_outputLocationFNR);
    m_outputLocationFNR->setFilter(QDir::Dirs);

    vLay->addLayout(outputLayout);

    m_progressLabelLayout = new QVBoxLayout(labels);
    vLay->addWidget(labels);
    m_progressBar = new QProgressBar;
    vLay->addWidget(m_progressBar);
    m_resultList = new ResultListWidget;
    vLay->addWidget(m_resultList);

    resize(QSize(640, 480));

    m_tasks = coll;
    assert(m_tasks);
    m_resultList->setTaskCollection(coll);
    connect(m_tasks.get(), &TaskCollection::progress, this, &DecryptVerifyFilesDialog::progress);
    connect(m_tasks.get(), &TaskCollection::done, this, &DecryptVerifyFilesDialog::allDone);
    connect(m_tasks.get(), &TaskCollection::result, this, &DecryptVerifyFilesDialog::result);
    connect(m_tasks.get(), &TaskCollection::started, this, &DecryptVerifyFilesDialog::started);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttonBox, &QDialogButtonBox::clicked, this, &DecryptVerifyFilesDialog::btnClicked);

    layout()->addWidget(m_buttonBox);

    bool hasOutputs = false;
    Q_FOREACH (const boost::shared_ptr<Task> t, coll->tasks()) {
        if (!qobject_cast<VerifyDetachedTask *>(t.get())) {
            hasOutputs = true;
            break;
        }
    }
    if (hasOutputs) {
        setWindowTitle(i18n("Decrypt/Verify Files"));
        m_okButton = QDialogButtonBox::SaveAll;
        m_buttonBox->addButton(QDialogButtonBox::Discard);
        connect(m_buttonBox, &QDialogButtonBox::accepted, this, &DecryptVerifyFilesDialog::checkAccept);
    } else {
        outLabel->setVisible(false);
        m_outputLocationFNR->setVisible(false);
        setWindowTitle(i18n("Verify Files"));
        m_okButton = QDialogButtonBox::Ok;
        m_buttonBox->addButton(QDialogButtonBox::Close);
        connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accepted);
    }
    m_buttonBox->addButton(m_okButton);
    m_buttonBox->button(m_okButton)->setEnabled(false);
}

void DecryptVerifyFilesDialog::allDone()
{
    qCDebug(KLEOPATRA_LOG) << "All done";
    Q_ASSERT(m_tasks);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);
    m_tasks.reset();
    Q_FOREACH (const QString &i, m_progressLabelByTag.keys()) {
        if (!i.isEmpty()) {
            m_progressLabelByTag.value(i)->setText(i18n("%1: All operations completed.", i));
        } else {
            m_progressLabelByTag.value(i)->setText(i18n("All operations completed."));
        }
    }
    m_buttonBox->button(m_okButton)->setEnabled(true);
}

void DecryptVerifyFilesDialog::started(const boost::shared_ptr<Task> &task)
{
    Q_ASSERT(task);
    const auto tag = task->tag();
    auto label = labelForTag(tag);
    Q_ASSERT(label);
    if (tag.isEmpty()) {
        label->setText(i18nc("number, operation description", "Operation %1: %2", m_tasks->numberOfCompletedTasks() + 1, task->label()));
    } else {
        label->setText(i18nc("tag( \"OpenPGP\" or \"CMS\"),  operation description", "%1: %2", tag, task->label()));
    }
}

QLabel *DecryptVerifyFilesDialog::labelForTag(const QString &tag)
{
    if (QLabel *const label = m_progressLabelByTag.value(tag)) {
        return label;
    }
    auto label = new QLabel;
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    m_progressLabelLayout->addWidget(label);
    m_progressLabelByTag.insert(tag, label);
    return label;
}

void DecryptVerifyFilesDialog::progress(const QString &msg, int progress, int total)
{
    Q_UNUSED(msg);
    assert(progress >= 0);
    assert(total >= 0);
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(progress);
}

void DecryptVerifyFilesDialog::setOutputLocation(const QString &dir)
{
    m_outputLocationFNR->setFileName(dir);
}

QString DecryptVerifyFilesDialog::outputLocation() const
{
    return m_outputLocationFNR->fileName();
}

void DecryptVerifyFilesDialog::btnClicked(QAbstractButton *btn)
{
    if (m_buttonBox->buttonRole(btn) == QDialogButtonBox::DestructiveRole) {
        close();
    }
}

void DecryptVerifyFilesDialog::checkAccept() {
    const auto outLoc = outputLocation();
    if (outLoc.isEmpty()) {
        KMessageBox::information(this, i18n("Please select an output folder."),
                                 i18n("No output folder."));
        return;
    }
    const QFileInfo fi(outLoc);

    if (fi.exists() && fi.isDir() && fi.isWritable()) {
        accept();
        return;
    }

    if (!fi.exists()) {
        qCDebug(KLEOPATRA_LOG) << "Output dir does not exist. Trying to create.";
        const QDir dir(outLoc);
        if (!dir.mkdir(outLoc)) {
            KMessageBox::information(this, i18n("Please select a different output folder."),
                                     i18n("Failed to create output folder."));
            return;
        } else {
            accept();
            return;
        }
    }

    KMessageBox::information(this, i18n("Please select a different output folder."),
                             i18n("Invalid output folder."));
}
