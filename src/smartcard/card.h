#ifndef SMARTCARD_CARD_H
#define SMARTCARD_CARD_H
/*  smartcard/card.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2017 Intevation GmbH

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

namespace Kleo
{
namespace SmartCard
{
/** Class to work with Smartcards or other Hardware tokens. */
class Card
{
public:
    enum AppType {
        UnknownApplication,
        OpenPGPApplication,
        NksApplication,
        P15Application,
        DinSigApplication,
        GeldkarteApplication,

        NumAppTypes
    };

    enum PinState {
        UnknownPinState,
        NullPin,
        PinBlocked,
        NoPin,
        PinOk,

        NumPinStates
    };

    enum Status {
        NoCard,
        CardPresent,
        CardActive,
        CardUsable,

        _NumScdStates,

        CardCanLearnKeys = _NumScdStates,
        CardHasNullPin,

        CardError,

        NumStates
    };

    Card();
    Card(Status s);

    Status status;
    std::string serialNumber;
    AppType appType;
    int appVersion;
    std::vector<PinState> pinStates;
    int slot;
};
} // namespace Smartcard
} // namespace Kleopatra
#endif // SMARTCARD_CARD_H
