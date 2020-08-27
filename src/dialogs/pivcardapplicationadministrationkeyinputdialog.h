/*  dialogs/pivcardapplicationadministrationkeyinputdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef DIALOGS_PIVCARDAPPLICATIONADMINISTRATIONKEYINPUTDIALOG_H
#define DIALOGS_PIVCARDAPPLICATIONADMINISTRATIONKEYINPUTDIALOG_H

#include <QDialog>

namespace Kleo
{
namespace Dialogs
{
class PIVCardApplicationAdministrationKeyInputDialog: public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QString labelText READ labelText WRITE setLabelText)

public:
    explicit PIVCardApplicationAdministrationKeyInputDialog(QWidget *parent = nullptr);

    void setLabelText(const QString& text);
    QString labelText() const;

    QByteArray adminKey() const;

private:
    class Private;
    std::shared_ptr<Private> d;
};
} // namespace Dialogs
} // namespace Kleo


#endif // DIALOGS_PIVCARDAPPLICATIONADMINISTRATIONKEYINPUTDIALOG_H
