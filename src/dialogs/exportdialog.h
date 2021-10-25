/*  SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>
#include <QDialog>

namespace GpgME {
class Key;
class Subkey;
class Error;
}

namespace Kleo {

class ExportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ExportWidget(QWidget *parent = nullptr);
    ~ExportWidget() override;

    void setKey(const GpgME::Key &key, unsigned int flags = 0);
    void setKey(const GpgME::Subkey &key, unsigned int flags = 0);
    GpgME::Key key() const;

private Q_SLOTS:
    void exportResult(const GpgME::Error &err, const QByteArray &data);

private:
    class Private;
    const QScopedPointer<Private> d;
};


class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(QWidget *parent = nullptr);
    ~ExportDialog() override;

    void setKey(const GpgME::Key &key, unsigned int flags = 0);
    void setKey(const GpgME::Subkey &key, unsigned int flags = 0);
    GpgME::Key key() const;

private:
    ExportWidget *mWidget;
};

} // namespace Kleo
