/*
    cryptooperationsconfigwidget.h

    This file is part of kleopatra, the KDE key manager
    SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "labelledwidget.h"

#include <QWidget>

#include <utils/pimpl_ptr.h>

class QCheckBox;
class QComboBox;
class QBoxLayout;
class QPushButton;

namespace Kleo
{
class FileOperationsPreferences;
class Settings;

namespace Config
{

class CryptoOperationsConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CryptoOperationsConfigWidget(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~CryptoOperationsConfigWidget() override;

    void load();
    void save();
    void defaults();

Q_SIGNALS:
    void changed();

private:
    void setupGui();

    void load(const Kleo::FileOperationsPreferences &filePrefs,
              const Kleo::Settings &settings);

private:
    QCheckBox *mPGPFileExtCB = nullptr;
    QCheckBox *mAutoDecryptVerifyCB = nullptr;
    QCheckBox *mAutoExtractArchivesCB = nullptr;
    QCheckBox *mASCIIArmorCB = nullptr;
    QCheckBox *mTmpDirCB = nullptr;
    QCheckBox *mSymmetricOnlyCB = nullptr;
    Kleo::LabelledWidget<QComboBox> mChecksumDefinitionCB;
    Kleo::LabelledWidget<QComboBox> mArchiveDefinitionCB;
    QPushButton *mApplyBtn = nullptr;
};

}
}

