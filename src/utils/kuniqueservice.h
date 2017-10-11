#ifndef KUNIQUESERVICE_H
#define KUNIQUESERVICE_H
/*
    kuniqueservice.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2016 by Bundesamt für Sicherheit in der Informationstechnik
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
#include <QObject>
#include <QString>
#include <QStringList>

/**
 * This class can be used to create a unique service and redirect calls
 * to this service similarly to KDBusService(KDBusService::Unique).
 * @code
 *   YourApplication app(argc, argv);
 *
 *   KUniqueService service;
 *   QObject::connect(&service, &KUniqueService::activateRequested,
 *                    &app, &YourApplication::slotActivateRequested);
 *   QObject::connect(&app, &YourApplication::setExitValue,
 *   &service, [&service](int i) {
 *       service.setExitValue(i);
 *   });
 * @endcode
 * This will redirect calls to running instances over the
 * the slotActivateRequested. When you set the exit
 * value the calling process will afterwards exit with the
 * code provided.
 * If you do not set the exit value the application will not
 * be terminated.
 * @author Andre Heinecke (aheinecke@intevation.org)
 */
class KUniqueService : public QObject
{
    Q_OBJECT
public:
    /**
     * Default constructor
     */
    KUniqueService();
    ~KUniqueService();

public Q_SLOTS:
    /**
     * Set the exit @p code the second app should use to terminate.
     * If unset it exits with 0.
     * @param code The exit code.
     */
    void setExitValue(int code);

Q_SIGNALS:
    void activateRequested(const QStringList &arguments,
                           const QString &workingDirectory);

private:
    void emitActivateRequested(const QStringList &arguments,
                               const QString &workingDirectory)
    {
        Q_EMIT activateRequested(arguments, workingDirectory);
    }
    class KUniqueServicePrivate;
    Q_DECLARE_PRIVATE(KUniqueService)
    KUniqueServicePrivate *d_ptr;
};
#endif // KUNIQUESERVICE_H
