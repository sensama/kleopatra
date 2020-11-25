/* -*- mode: c++; c-basic-offset:4 -*-
    dialogs/csrcreationresultdialog.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KLEOPATRA_DIALOGS_CSRCREATIONRESULTDIALOG_H__
#define __KLEOPATRA_DIALOGS_CSRCREATIONRESULTDIALOG_H__

#include <QDialog>

namespace Kleo
{
namespace Dialogs
{

class CSRCreationResultDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CSRCreationResultDialog(QWidget *parent = nullptr);
    ~CSRCreationResultDialog() override;

    void setCSR(const QByteArray &csr);

private:
    class Private;
    const std::unique_ptr<Private> d;
};

}
}

#endif // __KLEOPATRA_DIALOGS_CSRCREATIONRESULTDIALOG_H__
