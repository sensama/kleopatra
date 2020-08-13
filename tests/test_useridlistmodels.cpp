/* -*- mode: c++; c-basic-offset:4 -*-
    test_useridlistmodels.cpp

    This file is part of Kleopatra's test suite.
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include <models/useridlistmodel.h>

#include <KAboutData>

#include <QTreeView>

#ifdef KLEO_MODEL_TEST
# include <models/modeltest.h>
#endif

#include <qgpgme/eventloopinteractor.h>

#include <gpgme++/context.h>
#include <gpgme++/error.h>
#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>

#include <gpg-error.h>

#include <memory>
#include <stdexcept>
#include <vector>
#include <string>
#include <iostream>
#include <QApplication>
#include <KLocalizedString>
#include <QCommandLineParser>
#include <QCommandLineOption>

class KeyResolveJob : QObject
{
    Q_OBJECT
public:
    explicit KeyResolveJob(GpgME::Protocol proto = GpgME::OpenPGP, QObject *p = 0)
        : QObject(p),
          m_ctx(GpgME::Context::createForProtocol(proto)),
          m_done(false),
          m_loop(0)
    {
        Q_ASSERT(m_ctx.get());
        connect(QGpgME::EventLoopInteractor::instance(), SIGNAL(nextKeyEventSignal(GpgME::Context*,GpgME::Key)),
                this, SLOT(slotNextKey(GpgME::Context*,GpgME::Key)));
        connect(QGpgME::EventLoopInteractor::instance(), SIGNAL(operationDoneEventSignal(GpgME::Context*,GpgME::Error)),
                this, SLOT(slotDone(GpgME::Context*,GpgME::Error)));

        m_ctx->setManagedByEventLoopInteractor(true);
    }

    GpgME::Error start(const char *pattern, bool secretOnly = false)
    {
        m_ctx->addKeyListMode(GpgME::Signatures | GpgME::SignatureNotations);
        return m_ctx->startKeyListing(pattern, secretOnly);
    }

    GpgME::Error waitForDone()
    {
        if (m_done) {
            return m_error;
        }
        QEventLoop loop;
        m_loop = &loop;
        loop.exec();
        m_loop = 0;
        return m_error;
    }

    std::vector<GpgME::Key> keys() const
    {
        return m_keys;
    }

private Q_SLOTS:
    void slotNextKey(GpgME::Context *ctx, const GpgME::Key &key)
    {
        if (ctx != m_ctx.get()) {
            return;
        }
        m_keys.push_back(key);
    }
    void slotDone(GpgME::Context *ctx, const GpgME::Error &err)
    {
        if (ctx != m_ctx.get()) {
            return;
        }
        m_error = err;
        m_done = true;
        if (m_loop) {
            m_loop->quit();
        }
    }

private:
    std::auto_ptr<GpgME::Context> m_ctx;
    GpgME::Error m_error;
    bool m_done;
    std::vector<GpgME::Key> m_keys;
    QEventLoop *m_loop;
};

using namespace GpgME;
using namespace Kleo;

static void start(const QString &str, Protocol proto)
{
    const QByteArray arg = str.toUtf8();

    KeyResolveJob job(proto);

    if (const GpgME::Error err = job.start(arg)) {
        throw std::runtime_error(std::string("startKeyListing: ") + gpg_strerror(err.encodedError()));
    }

    if (const GpgME::Error err = job.waitForDone()) {
        throw std::runtime_error(std::string("nextKey: ") + gpg_strerror(err.encodedError()));
    }

    const Key key = job.keys().front();

    if (key.isNull()) {
        throw std::runtime_error(std::string("key is null"));
    }

    QTreeView *const tv = new QTreeView;
    tv->setWindowTitle(QString::fromLatin1("UserIDListModel Test - %1").arg(str));

    UserIDListModel *const model = new UserIDListModel(tv);
#ifdef KLEO_MODEL_TEST
    new ModelTest(model);
#endif
    model->setKey(key);

    tv->setModel(model);

    tv->show();
}

int main(int argc, char *argv[])
{

    KAboutData aboutData(QStringLiteral("test_useridlistmodels"), i18n("UserIDListModel Test"), QStringLiteral("0.1"));
    QApplication app(argc, argv);
    QCommandLineParser parser;
    KAboutData::setApplicationData(aboutData);
    parser.addVersionOption();
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList() <<  QStringLiteral("p"), i18n("OpenPGP certificate to look up"), QStringLiteral("pattern")));
    parser.addOption(QCommandLineOption(QStringList() <<  QStringLiteral("x"), i18n("X.509 certificate to look up"), QStringLiteral("pattern")));

    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    if (parser.values(QStringLiteral("p")).empty() && parser.values(QStringLiteral("x")).empty()) {
        return 1;
    }

    try {

        Q_FOREACH (const QString &arg, parser.values(QStringLiteral("p"))) {
            start(arg, OpenPGP);
        }

        Q_FOREACH (const QString &arg, parser.values(QStringLiteral("x"))) {
            start(arg, CMS);
        }

        return app.exec();

    } catch (const std::exception &e) {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return 1;
    }
}

#include "test_useridlistmodel.moc"
