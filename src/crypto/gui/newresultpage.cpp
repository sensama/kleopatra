/* -*- mode: c++; c-basic-offset:4 -*-
    crypto/gui/resultpage.cpp

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

#include "newresultpage.h"

#include "resultlistwidget.h"
#include "resultitemwidget.h"

#include <crypto/taskcollection.h>

#include <Libkleo/Stl_Util>

#include <KLocalizedString>

#include <QCheckBox>
#include <QHash>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QTimer>


static const int ProgressBarHideDelay = 2000; // 2 secs

using namespace Kleo;
using namespace Kleo::Crypto;
using namespace Kleo::Crypto::Gui;

class NewResultPage::Private
{
    NewResultPage *const q;
public:
    explicit Private(NewResultPage *qq);

    void progress(const QString &msg, int progress, int total);
    void result(const std::shared_ptr<const Task::Result> &result);
    void started(const std::shared_ptr<Task> &result);
    void allDone();
    QLabel *labelForTag(const QString &tag);

    std::vector< std::shared_ptr<TaskCollection> > m_collections;
    QTimer m_hideProgressTimer;
    QProgressBar *m_progressBar;
    QHash<QString, QLabel *> m_progressLabelByTag;
    QVBoxLayout *m_progressLabelLayout;
    int m_lastErrorItemIndex;
    ResultListWidget *m_resultList;
};

NewResultPage::Private::Private(NewResultPage *qq) : q(qq), m_lastErrorItemIndex(0)
{
    m_hideProgressTimer.setInterval(ProgressBarHideDelay);
    m_hideProgressTimer.setSingleShot(true);

    QBoxLayout *const layout = new QVBoxLayout(q);
    QWidget *const labels = new QWidget;
    m_progressLabelLayout = new QVBoxLayout(labels);
    layout->addWidget(labels);
    m_progressBar = new QProgressBar;
    layout->addWidget(m_progressBar);
    m_resultList = new ResultListWidget;
    connect(m_resultList, &ResultListWidget::linkActivated, q, &NewResultPage::linkActivated);
    layout->addWidget(m_resultList, 1);

    connect(&m_hideProgressTimer, &QTimer::timeout, m_progressBar, &QProgressBar::hide);
}

void NewResultPage::Private::progress(const QString &msg, int progress, int total)
{
    Q_UNUSED(msg);
    Q_ASSERT(progress >= 0);
    Q_ASSERT(total >= 0);
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(progress);
}

void NewResultPage::Private::allDone()
{
    Q_ASSERT(!m_collections.empty());
    if (!m_resultList->isComplete()) {
        return;
    }
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);
    m_collections.clear();
    Q_FOREACH (const QString &i, m_progressLabelByTag.keys()) {
        if (!i.isEmpty()) {
            m_progressLabelByTag.value(i)->setText(i18n("%1: All operations completed.", i));
        } else {
            m_progressLabelByTag.value(i)->setText(i18n("All operations completed."));
        }
    }
    if (QAbstractButton *cancel = q->wizard()->button(QWizard::CancelButton)) {
        cancel->setEnabled(false);
    }
    Q_EMIT q->completeChanged();
    m_hideProgressTimer.start();
}

void NewResultPage::Private::result(const std::shared_ptr<const Task::Result> &)
{
}

void NewResultPage::Private::started(const std::shared_ptr<Task> &task)
{
    Q_ASSERT(task);
    const QString tag = task->tag();
    QLabel *const label = labelForTag(tag);
    Q_ASSERT(label);
    if (tag.isEmpty()) {
        label->setText(i18nc("number, operation description", "Operation %1: %2", m_resultList->numberOfCompletedTasks() + 1, task->label()));
    } else {
        label->setText(i18nc("tag( \"OpenPGP\" or \"CMS\"),  operation description", "%1: %2", tag, task->label()));
    }
}

NewResultPage::NewResultPage(QWidget *parent) : QWizardPage(parent), d(new Private(this))
{
    setTitle(i18n("<b>Results</b>"));
}

NewResultPage::~NewResultPage()
{
}

void NewResultPage::setTaskCollection(const std::shared_ptr<TaskCollection> &coll)
{
    //clear(); ### PENDING(marc) implement
    addTaskCollection(coll);
}

void NewResultPage::addTaskCollection(const std::shared_ptr<TaskCollection> &coll)
{
    Q_ASSERT(coll);
    if (std::find(d->m_collections.cbegin(), d->m_collections.cend(), coll) != d->m_collections.cend()) {
        return;
    }
    d->m_hideProgressTimer.stop();
    d->m_progressBar->show();
    d->m_collections.push_back(coll);
    d->m_resultList->addTaskCollection(coll);
    connect(coll.get(), SIGNAL(progress(QString,int,int)),
            this, SLOT(progress(QString,int,int)));
    connect(coll.get(), SIGNAL(done()),
            this, SLOT(allDone()));
    connect(coll.get(), SIGNAL(result(std::shared_ptr<const Kleo::Crypto::Task::Result>)),
            this, SLOT(result(std::shared_ptr<const Kleo::Crypto::Task::Result>)));
    connect(coll.get(), SIGNAL(started(std::shared_ptr<Kleo::Crypto::Task>)),
            this, SLOT(started(std::shared_ptr<Kleo::Crypto::Task>)));

    Q_FOREACH (const std::shared_ptr<Task> &i, coll->tasks()) {    // create labels for all tags in collection
        Q_ASSERT(i);
        QLabel *l = d->labelForTag(i->tag());
        Q_ASSERT(l); (void)l;
    }
    Q_EMIT completeChanged();
}

QLabel *NewResultPage::Private::labelForTag(const QString &tag)
{
    if (QLabel *const label = m_progressLabelByTag.value(tag)) {
        return label;
    }
    QLabel *label = new QLabel;
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    m_progressLabelLayout->addWidget(label);
    m_progressLabelByTag.insert(tag, label);
    return label;
}

bool NewResultPage::isComplete() const
{
    return d->m_resultList->isComplete();
}

#include "moc_newresultpage.cpp"
