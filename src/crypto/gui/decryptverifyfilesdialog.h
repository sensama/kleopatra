/* crypto/gui/decryptverifyfilesdialog.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB
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
#ifndef CRYPTO_GUI_DECRYPTVERIFYFILESDIALOG_H
#define CRYPTO_GUI_DECRYPTVERIFYFILESDIALOG_H

#include <QDialog>
#include <QString>
#include <QDialogButtonBox>
#include <QHash>
#include "crypto/task.h"

class QVBoxLayout;
class QProgressBar;
template <typename K, typename U> class QHash;
class QLabel;

namespace Kleo
{
class FileNameRequester;
namespace Crypto
{
class TaskCollection;

namespace Gui
{
class ResultListWidget;

class DecryptVerifyFilesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DecryptVerifyFilesDialog(const std::shared_ptr<TaskCollection> &coll,
                                      QWidget *parent = nullptr);
    ~DecryptVerifyFilesDialog();

    void setOutputLocation(const QString &dir);
    QString outputLocation() const;

protected Q_SLOTS:
    void progress(const QString &msg, int progress, int total);
    void started(const std::shared_ptr<Task> &result);
    void allDone();
    void btnClicked(QAbstractButton *btn);
    void checkAccept();

protected:
    void readConfig();
    void writeConfig();

protected:
    QLabel *labelForTag(const QString &tag);

private:
    std::shared_ptr<TaskCollection> m_tasks;
    QProgressBar *m_progressBar;
    QHash<QString, QLabel *> m_progressLabelByTag;
    QVBoxLayout *m_progressLabelLayout;
    int m_lastErrorItemIndex;
    ResultListWidget *m_resultList;
    FileNameRequester *m_outputLocationFNR;
    QDialogButtonBox::StandardButton m_saveButton;
    QDialogButtonBox *m_buttonBox;
};

} // namespace Gui
} //namespace Crypto;
} // namespace Kleo

#endif // CRYPTO_GUI_DECRYPTVERIFYFILESDIALOG_H
