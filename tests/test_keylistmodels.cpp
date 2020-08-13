/* -*- mode: c++; c-basic-offset:4 -*-
    test_keylistmodels.cpp

    This file is part of Kleopatra's test suite.
    SPDX-FileCopyrightText: 2007 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include <models/keylistmodel.h>
#include <models/keylistsortfilterproxymodel.h>

#include <utils/formatting.h>

#include <KAboutData>

#include <QTreeView>
#include <QLineEdit>
#include <QTimer>
#include <QEventLoop>
#include <QDateTime>
#include "kleopatra_debug.h"

#include <qgpgme/eventloopinteractor.h>

#include <gpgme++/context.h>
#include <gpgme++/error.h>
#include <gpgme++/key.h>

#include <memory>
#include <vector>
#include <string>
#include <QApplication>
#include <KLocalizedString>
#include <QCommandLineParser>
#include <QCommandLineOption>

class Relay : public QObject
{
    Q_OBJECT
public:
    explicit Relay(QObject *p = nullptr) : QObject(p) {}

public Q_SLOTS:
    void slotNextKeyEvent(GpgME::Context *, const GpgME::Key &key)
    {
        qDebug("next key");
        mKeys.push_back(key);
        // push out keys in chunks of 1..16 keys
        if (mKeys.size() > qrand() % 16U) {
            emit nextKeys(mKeys);
            mKeys.clear();
        }
    }

    void slotOperationDoneEvent(GpgME::Context *, const GpgME::Error &error)
    {
        qDebug("listing done error: %d", error.encodedError());
    }

Q_SIGNALS:
    void nextKeys(const std::vector<GpgME::Key> &keys);

private:
    std::vector<GpgME::Key> mKeys;
};

int main(int argc, char *argv[])
{

    if (const GpgME::Error initError = GpgME::initializeLibrary(0)) {
        qCDebug(KLEOPATRA_LOG) << "Error initializing gpgme:" << QString::fromLocal8Bit(initError.asString());
        return 1;
    }

    KAboutData aboutData("test_flatkeylistmodel", 0, i18n("FlatKeyListModel Test"), "0.2");
    QApplication app(argc, argv);
    QCommandLineParser parser;
    KAboutData::setApplicationData(aboutData);
    parser.addVersionOption();
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList() <<  QStringLiteral("flat"), i18n("Perform flat certificate listing")));
    parser.addOption(QCommandLineOption(QStringList() <<  QStringLiteral("hierarchical"), i18n("Perform hierarchical certificate listing")));
    parser.addOption(QCommandLineOption(QStringList() <<  QStringLiteral("disable-smime"), i18n("Do not list SMIME certificates")));
    parser.addOption(QCommandLineOption(QStringList() <<  QStringLiteral("secret"), i18n("List secret keys only")));

    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    const bool showFlat = parser.isSet(QStringLiteral("flat")) || !parser.isSet(QStringLiteral("hierarchical"));
    const bool showHier = parser.isSet(QStringLiteral("hierarchical")) || !parser.isSet(QStringLiteral("flat"));
    const bool disablesmime = parser.isSet(QStringLiteral("disable-smime"));
    const bool secretOnly = parser.isSet(QStringLiteral("secret"));

    qsrand(QDateTime::currentDateTime().toTime_t());

    QWidget flatWidget, hierarchicalWidget;
    QVBoxLayout flatLay(&flatWidget), hierarchicalLay(&hierarchicalWidget);
    QLineEdit flatLE(&flatWidget), hierarchicalLE(&hierarchicalWidget);
    QTreeView flat(&flatWidget), hierarchical(&hierarchicalWidget);

    flat.setSortingEnabled(true);
    flat.sortByColumn(Kleo::AbstractKeyListModel::Fingerprint, Qt::AscendingOrder);
    hierarchical.setSortingEnabled(true);
    hierarchical.sortByColumn(Kleo::AbstractKeyListModel::Fingerprint, Qt::AscendingOrder);

    flatLay.addWidget(&flatLE);
    flatLay.addWidget(&flat);

    hierarchicalLay.addWidget(&hierarchicalLE);
    hierarchicalLay.addWidget(&hierarchical);

    flatWidget.setWindowTitle(QStringLiteral("Flat Key Listing"));
    hierarchicalWidget.setWindowTitle(QStringLiteral("Hierarchical Key Listing"));

    Kleo::KeyListSortFilterProxyModel flatProxy, hierarchicalProxy;

    QObject::connect(&flatLE, SIGNAL(textChanged(QString)), &flatProxy, SLOT(setFilterFixedString(QString)));
    QObject::connect(&hierarchicalLE, SIGNAL(textChanged(QString)), &hierarchicalProxy, SLOT(setFilterFixedString(QString)));

    Relay relay;
    QObject::connect(QGpgME::EventLoopInteractor::instance(), SIGNAL(nextKeyEventSignal(GpgME::Context*,GpgME::Key)),
                     &relay, SLOT(slotNextKeyEvent(GpgME::Context*,GpgME::Key)));
    QObject::connect(QGpgME::EventLoopInteractor::instance(), SIGNAL(operationDoneEventSignal(GpgME::Context*,GpgME::Error)),
                     &relay, SLOT(slotOperationDoneEvent(GpgME::Context*,GpgME::Error)));

    if (showFlat)
        if (Kleo::AbstractKeyListModel *const model = Kleo::AbstractKeyListModel::createFlatKeyListModel(&flat)) {
            QObject::connect(&relay, SIGNAL(nextKeys(std::vector<GpgME::Key>)), model, SLOT(addKeys(std::vector<GpgME::Key>)));
            model->setToolTipOptions(Kleo::Formatting::AllOptions);
            flatProxy.setSourceModel(model);
            flat.setModel(&flatProxy);

            flatWidget.show();
        }

    if (showHier)
        if (Kleo::AbstractKeyListModel *const model = Kleo::AbstractKeyListModel::createHierarchicalKeyListModel(&hierarchical)) {
            QObject::connect(&relay, SIGNAL(nextKeys(std::vector<GpgME::Key>)), model, SLOT(addKeys(std::vector<GpgME::Key>)));
            model->setToolTipOptions(Kleo::Formatting::AllOptions);
            hierarchicalProxy.setSourceModel(model);
            hierarchical.setModel(&hierarchicalProxy);

            hierarchicalWidget.show();
        }

    const char *pattern[] = { nullptr };

    const std::auto_ptr<GpgME::Context> pgp(GpgME::Context::createForProtocol(GpgME::OpenPGP));
    pgp->setManagedByEventLoopInteractor(true);
    pgp->setKeyListMode(GpgME::Local);

    if (const GpgME::Error e = pgp->startKeyListing(pattern, secretOnly)) {
        qCDebug(KLEOPATRA_LOG) << "pgp->startKeyListing() ->" << e.asString();
    }

    if (!disablesmime) {
        const std::auto_ptr<GpgME::Context> cms(GpgME::Context::createForProtocol(GpgME::CMS));
        cms->setManagedByEventLoopInteractor(true);
        cms->setKeyListMode(GpgME::Local);

        if (const GpgME::Error e = cms->startKeyListing(pattern, secretOnly)) {
            qCDebug(KLEOPATRA_LOG) << "cms" << e.asString();
        }

        QEventLoop loop;
        QTimer::singleShot(2000, &loop, SLOT(quit()));
        loop.exec();

        const std::auto_ptr<GpgME::Context> cms2(GpgME::Context::createForProtocol(GpgME::CMS));
        cms2->setManagedByEventLoopInteractor(true);
        cms2->setKeyListMode(GpgME::Local);

        if (const GpgME::Error e = cms2->startKeyListing(pattern, secretOnly)) {
            qCDebug(KLEOPATRA_LOG) << "cms2" << e.asString();
        }
    }

    return app.exec();
}

#include "test_flatkeylistmodel.moc"
