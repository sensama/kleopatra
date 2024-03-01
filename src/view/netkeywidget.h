/*  view/netkeywidget.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <Libkleo/Predicates>

#include <QWidget>

#include <gpgme++/error.h>

#include <set>
#include <string>
#include <vector>

class QLabel;
class QPushButton;
class QScrollArea;

namespace GpgME
{
class Key;
class KeyListResult;
}

namespace Kleo
{
class NullPinWidget;
class KeyTreeView;
class ProgressOverlay;

namespace SmartCard
{
class NetKeyCard;
} // namespace SmartCard

class NetKeyWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NetKeyWidget(QWidget *parent = nullptr);
    ~NetKeyWidget() override;

    void setCard(const SmartCard::NetKeyCard *card);

private:
    void loadCertificates();
    void ensureCertificatesAreValidated();
    void startCertificateValidation(const std::vector<GpgME::Key> &certificates);
    void certificateValidationDone(const GpgME::KeyListResult &result, const std::vector<GpgME::Key> &keys);
    void learnCard();
    void doChangePin(const std::string &keyRef);
    void createKeyFromCardKeys();
    void createCSR();

private:
    std::string mSerialNumber;
    std::vector<GpgME::Key> mCertificates;

    using KeySet = std::set<GpgME::Key, _detail::ByFingerprint<std::less>>;
    KeySet mValidatedCertificates;

    QLabel *mSerialNumberLabel = nullptr;
    QLabel *mVersionLabel = nullptr;
    QLabel *mErrorLabel = nullptr;
    NullPinWidget *mNullPinWidget = nullptr;
    QPushButton *mKeyForCardKeysButton = nullptr;
    QPushButton *mCreateCSRButton = nullptr;
    QPushButton *mChangeNKSPINBtn = nullptr;
    QPushButton *mChangeSigGPINBtn = nullptr;
    KeyTreeView *mTreeView = nullptr;
    ProgressOverlay *mTreeViewOverlay = nullptr;
    QScrollArea *mArea = nullptr;
};
} // namespace Kleo
