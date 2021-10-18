/*
    cryptooperationsconfigwidget.h

    This file is part of kleopatra, the KDE key manager
    SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QWidget>

#include <utils/pimpl_ptr.h>

class QCheckBox;
class QComboBox;
class QBoxLayout;
class QPushButton;

namespace Kleo
{
namespace Config
{

class CryptoOperationsConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CryptoOperationsConfigWidget(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~CryptoOperationsConfigWidget() override;

public Q_SLOTS:
    void load();
    void save();
    void defaults();

Q_SIGNALS:
    void changed();

private:
    void setupGui();
    void setupProfileGui(QBoxLayout *layout);
    void applyProfile(const QString &profile);

private:
    QCheckBox *mQuickEncryptCB,
              *mQuickSignCB,
              *mPGPFileExtCB,
              *mAutoDecryptVerifyCB,
              *mASCIIArmorCB,
              *mTmpDirCB,
              *mSymmetricOnlyCB;
    QComboBox *mChecksumDefinitionCB,
              *mArchiveDefinitionCB;
    QPushButton *mApplyBtn;
};

}
}

