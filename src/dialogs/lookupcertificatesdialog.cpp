/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/lookupcertificatesdialog.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "lookupcertificatesdialog.h"

#include <view/keytreeview.h>

#include <kleopatra_debug.h>

#include <Libkleo/KeyListModel>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSeparator>
#include <KSharedConfig>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTreeView>
#include <QVBoxLayout>

#include <gpgme++/key.h>

using namespace Kleo;
using namespace Kleo::Dialogs;
using namespace GpgME;

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
        Q_EMIT q->searchTextChanged(searchText());
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
        const auto *const model = dynamic_cast<KeyListModelInterface *>(view->model());
        Q_ASSERT(model);
        const QItemSelectionModel *const sm = view->selectionModel();
        Q_ASSERT(sm);
        return model->keys(sm->selectedRows());
    }

    int numSelectedCertificates() const
    {
        return ui.resultTV->selectedKeys().size();
    }

    QValidator *queryValidator();
    void updateQueryMode();

private:
    QueryMode queryMode = AnyQuery;
    bool passive;
    QValidator *anyQueryValidator = nullptr;
    QValidator *emailQueryValidator = nullptr;

    struct Ui {
        QLabel *guidanceLabel;
        QLabel *findLB;
        QLineEdit *findED;
        QPushButton *findPB;
        Kleo::KeyTreeView *resultTV;
        QPushButton *selectAllPB;
        QPushButton *deselectAllPB;
        QPushButton *detailsPB;
        QPushButton *saveAsPB;
        QDialogButtonBox *buttonBox;

        void setupUi(QDialog *dialog)
        {
            auto verticalLayout = new QVBoxLayout{dialog};
            auto gridLayout = new QGridLayout{};

            int row = 0;
            guidanceLabel = new QLabel{dialog};
            gridLayout->addWidget(guidanceLabel, row, 0, 1, 3);

            row++;
            findLB = new QLabel{i18n("Find:"), dialog};
            gridLayout->addWidget(findLB, row, 0, 1, 1);

            findED = new QLineEdit{dialog};
            findLB->setBuddy(findED);
            gridLayout->addWidget(findED, row, 1, 1, 1);

            findPB = new QPushButton{i18n("Search"), dialog};
            findPB->setAutoDefault(false);
            gridLayout->addWidget(findPB, row, 2, 1, 1);

            row++;
            gridLayout->addWidget(new KSeparator{Qt::Horizontal, dialog}, row, 0, 1, 3);

            row++;
            resultTV = new Kleo::KeyTreeView(dialog);
            resultTV->setEnabled(true);
            resultTV->setMinimumSize(QSize(400, 0));
            gridLayout->addWidget(resultTV, row, 0, 1, 2);

            auto buttonLayout = new QVBoxLayout{};

            selectAllPB = new QPushButton{i18n("Select All"), dialog};
            selectAllPB->setEnabled(false);
            selectAllPB->setAutoDefault(false);
            buttonLayout->addWidget(selectAllPB);

            deselectAllPB = new QPushButton{i18n("Deselect All"), dialog};
            deselectAllPB->setEnabled(false);
            deselectAllPB->setAutoDefault(false);
            buttonLayout->addWidget(deselectAllPB);

            buttonLayout->addStretch();

            detailsPB = new QPushButton{i18n("Details..."), dialog};
            detailsPB->setEnabled(false);
            detailsPB->setAutoDefault(false);
            buttonLayout->addWidget(detailsPB);

            saveAsPB = new QPushButton{i18n("Save As..."), dialog};
            saveAsPB->setEnabled(false);
            saveAsPB->setAutoDefault(false);
            buttonLayout->addWidget(saveAsPB);

            gridLayout->addLayout(buttonLayout, row, 2, 1, 1);

            verticalLayout->addLayout(gridLayout);

            buttonBox = new QDialogButtonBox{dialog};
            buttonBox->setStandardButtons(QDialogButtonBox::Close | QDialogButtonBox::Save);
            verticalLayout->addWidget(buttonBox);

            QObject::connect(findED, SIGNAL(returnPressed()), findPB, SLOT(animateClick()));
            QObject::connect(buttonBox, SIGNAL(accepted()), dialog, SLOT(accept()));
            QObject::connect(buttonBox, SIGNAL(rejected()), dialog, SLOT(reject()));
            QObject::connect(findPB, SIGNAL(clicked()), dialog, SLOT(slotSearchClicked()));
            QObject::connect(detailsPB, SIGNAL(clicked()), dialog, SLOT(slotDetailsClicked()));
            QObject::connect(saveAsPB, SIGNAL(clicked()), dialog, SLOT(slotSaveAsClicked()));
            QObject::connect(findED, SIGNAL(textChanged(QString)), dialog, SLOT(slotSearchTextChanged()));

            QMetaObject::connectSlotsByName(dialog);
        }

        explicit Ui(LookupCertificatesDialog *q)
        {
            q->setWindowTitle(i18n("Lookup on Server"));
            setupUi(q);

            saveAsPB->hide(); // ### not yet implemented in LookupCertificatesCommand

            findED->setClearButtonEnabled(true);

            resultTV->setFlatModel(AbstractKeyListModel::createFlatKeyListModel(q));
            resultTV->setHierarchicalView(false);

            importPB()->setText(i18n("Import"));
            importPB()->setEnabled(false);

            connect(resultTV->view(), SIGNAL(doubleClicked(QModelIndex)), q, SLOT(slotDetailsClicked()));

            findED->setFocus();

            connect(selectAllPB, &QPushButton::clicked, resultTV->view(), &QTreeView::selectAll);
            connect(deselectAllPB, &QPushButton::clicked, resultTV->view(), &QTreeView::clearSelection);
        }

        QPushButton *importPB() const
        {
            return buttonBox->button(QDialogButtonBox::Save);
        }
        QPushButton *closePB() const
        {
            return buttonBox->button(QDialogButtonBox::Close);
        }
    } ui;
};

LookupCertificatesDialog::Private::Private(LookupCertificatesDialog *qq)
    : q(qq)
    , passive(false)
    , ui(q)
{
    connect(ui.resultTV->view()->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), q, SLOT(slotSelectionChanged()));
    updateQueryMode();
}

LookupCertificatesDialog::Private::~Private()
{
}

void LookupCertificatesDialog::Private::readConfig()
{
    KConfigGroup configGroup(KSharedConfig::openStateConfig(), "LookupCertificatesDialog");

    KConfigGroup resultKeysConfig = configGroup.group("ResultKeysView");
    ui.resultTV->restoreLayout(resultKeysConfig);

    const QSize size = configGroup.readEntry("Size", QSize(600, 400));
    if (size.isValid()) {
        q->resize(size);
    }
}

void LookupCertificatesDialog::Private::writeConfig()
{
    KConfigGroup configGroup(KSharedConfig::openStateConfig(), "LookupCertificatesDialog");
    configGroup.writeEntry("Size", q->size());
    configGroup.sync();
}

static QString guidanceText(LookupCertificatesDialog::QueryMode mode)
{
    switch (mode) {
    default:
        qCWarning(KLEOPATRA_LOG) << __func__ << "Unknown query mode:" << mode;
        [[fallthrough]];
    case LookupCertificatesDialog::AnyQuery:
        return xi18nc("@info", "Enter a search term to search for matching certificates.");
    case LookupCertificatesDialog::EmailQuery:
        return xi18nc("@info", "Enter an email address to search for matching certificates.");
    };
}

QValidator *LookupCertificatesDialog::Private::queryValidator()
{
    switch (queryMode) {
    default:
        qCWarning(KLEOPATRA_LOG) << __func__ << "Unknown query mode:" << queryMode;
        [[fallthrough]];
    case AnyQuery: {
        if (!anyQueryValidator) {
            // allow any query with at least one non-whitespace character
            anyQueryValidator = new QRegularExpressionValidator{QRegularExpression{QStringLiteral(".*\\S+.*")}, q};
        }
        return anyQueryValidator;
    }
    case EmailQuery: {
        if (!emailQueryValidator) {
            // allow anything that looks remotely like an email address, i.e.
            // anything with an '@' surrounded by non-whitespace characters
            const QRegularExpression simpleEmailRegex{QStringLiteral(".*\\S+@\\S+.*")};
            emailQueryValidator = new QRegularExpressionValidator{simpleEmailRegex, q};
        }
        return emailQueryValidator;
    }
    }
}

void LookupCertificatesDialog::Private::updateQueryMode()
{
    ui.guidanceLabel->setText(guidanceText(queryMode));
    ui.findED->setValidator(queryValidator());
}

LookupCertificatesDialog::LookupCertificatesDialog(QWidget *p, Qt::WindowFlags f)
    : QDialog(p, f)
    , d(new Private(this))
{
    d->ui.findPB->setEnabled(false);
    d->readConfig();
}

LookupCertificatesDialog::~LookupCertificatesDialog()
{
    d->writeConfig();
}

void LookupCertificatesDialog::setQueryMode(QueryMode mode)
{
    d->queryMode = mode;
    d->updateQueryMode();
}

LookupCertificatesDialog::QueryMode LookupCertificatesDialog::queryMode() const
{
    return d->queryMode;
}

void LookupCertificatesDialog::setCertificates(const std::vector<Key> &certs)
{
    d->ui.resultTV->view()->setFocus();
    d->ui.resultTV->setKeys(certs);
    if (certs.size() == 1) {
        d->ui.resultTV->view()->setCurrentIndex(d->ui.resultTV->view()->model()->index(0, 0));
    }
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
    const QList<QObject *> list = q->children();
    for (QObject *const o : list) {
        if (QWidget *const w = qobject_cast<QWidget *>(o)) {
            w->setDisabled(passive && w != ui.closePB() && w != ui.buttonBox);
        }
    }

    if (passive) {
        return;
    }

    ui.findPB->setEnabled(ui.findED->hasAcceptableInput());

    const int n = q->selectedCertificates().size();

    ui.detailsPB->setEnabled(n == 1);
    ui.saveAsPB->setEnabled(n == 1);
    ui.importPB()->setEnabled(n != 0);
    ui.importPB()->setDefault(false); // otherwise Import becomes default button if enabled and return triggers both a search and accept()
}

#include "moc_lookupcertificatesdialog.cpp"
