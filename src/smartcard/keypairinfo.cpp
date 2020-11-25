/*  smartcard/keypairinfo.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "keypairinfo.h"

#include <QString>
#include <QStringList>

using namespace Kleo::SmartCard;

// static
KeyPairInfo KeyPairInfo::fromStatusLine(const std::string &s) {
    // The format of a KEYPAIRINFO line is
    //   KEYPAIRINFO <hexgrip> <keyref> [usage] [keytime] [algostr]
    // The string s does not contain the leading "KEYPAIRINFO ".
    KeyPairInfo info;
    const auto values = QString::fromStdString(s).split(QLatin1Char(' '));
    if (values.size() < 2) {
        return info;
    }
    info.grip = values[0].toStdString();
    info.keyRef = values[1].toStdString();
    if (values.size() >= 3) {
        info.usage = values[2].toStdString();
    }
    if (values.size() >= 4) {
        info.keyTime = values[3].toStdString();
    }
    if (values.size() >= 5) {
        info.algorithm = values[4].toStdString();
    }
    return info;
}

void KeyPairInfo::update(const KeyPairInfo &other)
{
    Q_ASSERT(keyRef == other.keyRef);
    if (keyRef != other.keyRef) {
        return;
    }
    if (grip != other.grip) {
        // reset all infos if the grip changed
        grip = other.grip;
        usage = std::string();
        keyTime = std::string();
        algorithm = std::string();
    }
    // now update all infos from other's infos unless other's infos are empty or not specified
    if (!other.usage.empty() && other.usage != "-") {
        usage = other.usage;
    }
    if (!other.keyTime.empty() && other.keyTime != "-") {
        keyTime = other.keyTime;
    }
    if (!other.algorithm.empty() && other.algorithm != "-") {
        algorithm = other.algorithm;
    }
}
