/*  dialogs/gencardkeydialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef DIALOGS_GENCARDKEYDIALOG_H
#define DIALOGS_GENCARDKEYDIALOG_H

#include <QDialog>

#include <gpgme++/key.h>

#include <memory>
#include <vector>

namespace Kleo
{
class GenCardKeyDialog: public QDialog
{
Q_OBJECT

public:
    struct KeyParams
    {
        QString name;
        QString email;
        QString comment;
        std::string algorithm;
        bool backup;
    };
    explicit GenCardKeyDialog(QWidget *parent = nullptr);

    KeyParams getKeyParams() const;

    void setSupportedAlgorithms(const std::vector<std::pair<std::string, QString>> &algorithms, const std::string &defaultAlgo);

private:
    class Private;
    std::shared_ptr<Private> d;
};
} // namespace Kleo


#endif // DIALOGS_GENCARDKEYDIALOG_H
