/*
    kleopatra_options.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2015 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

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
#ifndef KLEOPATRA_OPTIONS_H
#define KLEOPATRA_OPTIONS_H

#include <config-kleopatra.h>

#include <QCommandLineParser>
#include <KLocalizedString>

static void kleopatra_options(QCommandLineParser *parser)
{
    QList<QCommandLineOption> options;

    options << QCommandLineOption(QStringList() << QStringLiteral("openpgp")
                                  << QStringLiteral("p"),
                                  i18n("Use OpenPGP for the following operation"))
            << QCommandLineOption(QStringList() << QStringLiteral("cms")
                                  << QStringLiteral("c"),
                                  i18n("Use CMS (X.509, S/MIME) for the following operation"))
#ifdef HAVE_USABLE_ASSUAN
            << QCommandLineOption(QStringLiteral("uiserver-socket"),
                                  i18n("Location of the socket the ui server is listening on"),
                                  QStringLiteral("argument"))
            << QCommandLineOption(QStringLiteral("daemon"),
                                  i18n("Run UI server only, hide main window"))
#endif
            << QCommandLineOption(QStringList() << QStringLiteral("import-certificate")
                                  << QStringLiteral("i"),
                                  i18n("Import certificate file(s)"))
            << QCommandLineOption(QStringList() << QStringLiteral("encrypt")
                                  << QStringLiteral("e"),
                                  i18n("Encrypt file(s)"))
            << QCommandLineOption(QStringList() << QStringLiteral("sign")
                                  << QStringLiteral("s"),
                                  i18n("Sign file(s)"))
            << QCommandLineOption(QStringList() << QStringLiteral("sign-encrypt")
                                  << QStringLiteral("E"),
                                  i18n("Sign and/or encrypt file(s)"))
            << QCommandLineOption(QStringLiteral("encrypt-sign"),
                                  i18n("Same as --sign-encrypt, do not use"))
            << QCommandLineOption(QStringList() << QStringLiteral("decrypt")
                                  << QStringLiteral("d"),
                                  i18n("Decrypt file(s)"))
            << QCommandLineOption(QStringList() << QStringLiteral("verify")
                                  << QStringLiteral("V"),
                                  i18n("Verify file/signature"))
            << QCommandLineOption(QStringList() << QStringLiteral("decrypt-verify")
                                  << QStringLiteral("D"),
                                  i18n("Decrypt and/or verify file(s)"))
            << QCommandLineOption(QStringList() << QStringLiteral("search"),
                                  i18n("Search for a certificate on a keyserver"))
            << QCommandLineOption(QStringList() << QStringLiteral("checksum"),
                                  i18n("Create or check a checksum file"))
            << QCommandLineOption(QStringList() << QStringLiteral("query")
                                  << QStringLiteral("q"),
                                  i18nc("If a certificate is already known it shows the certificate details dialog."
                                        "Otherwise it brings up the certificate search dialog.",
                                        "Show details of a local certificate or search for it on a keyserver"
                                        " by fingerprint"))
            << QCommandLineOption(QStringList() << QStringLiteral("gen-key"),
                                  i18n("Create a new key pair or certificate signing request"))
            << QCommandLineOption(QStringLiteral("parent-windowid"),
                                  i18n("Parent Window Id for dialogs"),
                                  QStringLiteral("windowId"))
            << QCommandLineOption(QStringList() << QStringLiteral("config"),
                                  i18n("Open the config dialog"));

    parser->addOptions(options);

    /* Security note: To avoid code execution by shared library injection
     * through e.g. -platformpluginpath any external input should be seperated
     * by a double dash -- this is why query / search uses positional arguments.
     *
     * For example on Windows there is an URLhandler for openpgp4fpr:
     * be opened with Kleopatra's query function. And while a browser should
     * urlescape such a query there might be tricks to inject a quote character
     * and as such inject command line options for Kleopatra in an URL. */
    parser->addPositionalArgument(QStringLiteral("files"),
                                  i18n("File(s) to process"),
                                  QStringLiteral("-- [files..]"));
    parser->addPositionalArgument(QStringLiteral("query"),
                                  i18n("String or Fingerprint for query and search"),
                                  QStringLiteral("-- [query..]"));
}
#endif
