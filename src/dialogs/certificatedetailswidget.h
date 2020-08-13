/*  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KLEO_CERTIFICATEDETAILS_WIDGET_H
#define KLEO_CERTIFICATEDETAILS_WIDGET_H

#include <QWidget>
#include <QDialog>

#include <vector>

namespace GpgME {
class Key;
class Error;
class KeyListResult;
}

class CertificateDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CertificateDetailsWidget(QWidget *parent = nullptr);
    ~CertificateDetailsWidget();

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    class Private;
    const QScopedPointer<Private> d;

    // Windows QGpgME new style connect problem makes this necessary.
    Q_PRIVATE_SLOT(d, void keyListDone(const GpgME::KeyListResult &,
                   const std::vector<GpgME::Key> &, const QString &,
                   const GpgME::Error &))
};


class CertificateDetailsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CertificateDetailsDialog(QWidget *parent = nullptr);
    ~CertificateDetailsDialog();

    void setKey(const GpgME::Key &key);
    GpgME::Key key() const;

private:
    void readConfig();
    void writeConfig();
};

#endif
