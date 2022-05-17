/* -*- mode: c++; c-basic-offset:4 -*-
    newcertificatewizard/enterdetailspage_p.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB
    SPDX-FileCopyrightText: 2016, 2017 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "wizardpage_p.h"

class AdvancedSettingsDialog;
class QLineEdit;

class EnterDetailsPage : public Kleo::NewCertificateUi::WizardPage
{
    Q_OBJECT

public:
    struct Line {
        QString attr;
        QString label;
        QString regex;
        QLineEdit *edit;
    };

    explicit EnterDetailsPage(QWidget *parent = nullptr);
    ~EnterDetailsPage() override;

    bool isComplete() const override;
    void initializePage() override;
    void cleanupPage() override;

private:
    void updateForm();
    void clearForm();
    void saveValues();
    void registerDialogPropertiesAsFields();

private:
    QString pgpUserID() const;
    QString cmsDN() const;

private Q_SLOTS:
    void slotAdvancedSettingsClicked();
    void slotUpdateResultLabel();

private:
    struct UI;
    std::unique_ptr<UI> ui;

    QVector<Line> lineList;
    QList<QWidget *> dynamicWidgets;
    QMap<QString, QString> savedValues;
    AdvancedSettingsDialog *dialog;
};
