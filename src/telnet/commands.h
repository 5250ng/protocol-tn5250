// protocol-tn5250 - Standalone TN5250 protocol library
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>
#include <string>

namespace telnet {

// Telnet command codes (RFC 854)
enum class TelnetCommand : uint8_t {
    EOR = 239,
    SE = 240,
    NOP = 241,
    DM = 242,
    BRK = 243,
    IP = 244,
    AO = 245,
    AYT = 246,
    EC = 247,
    EL = 248,
    GA = 249,
    SB = 250,
    WILL = 251,
    WONT = 252,
    DO = 253,
    DONT = 254,
    IAC = 255
};

constexpr bool isTelnetCommand(uint8_t byte) {
    switch (byte) {
    case static_cast<uint8_t>(TelnetCommand::EOR):
    case static_cast<uint8_t>(TelnetCommand::SE):
    case static_cast<uint8_t>(TelnetCommand::NOP):
    case static_cast<uint8_t>(TelnetCommand::DM):
    case static_cast<uint8_t>(TelnetCommand::BRK):
    case static_cast<uint8_t>(TelnetCommand::IP):
    case static_cast<uint8_t>(TelnetCommand::AO):
    case static_cast<uint8_t>(TelnetCommand::AYT):
    case static_cast<uint8_t>(TelnetCommand::EC):
    case static_cast<uint8_t>(TelnetCommand::EL):
    case static_cast<uint8_t>(TelnetCommand::GA):
    case static_cast<uint8_t>(TelnetCommand::SB):
    case static_cast<uint8_t>(TelnetCommand::WILL):
    case static_cast<uint8_t>(TelnetCommand::WONT):
    case static_cast<uint8_t>(TelnetCommand::DO):
    case static_cast<uint8_t>(TelnetCommand::DONT):
    case static_cast<uint8_t>(TelnetCommand::IAC):
        return true;
    default:
        return false;
    }
}

constexpr bool isTelnetCommand(TelnetCommand cmd) {
    return isTelnetCommand(static_cast<uint8_t>(cmd));
}

constexpr bool isStandaloneTelnetCommand(uint8_t byte) {
    switch (byte) {
    case static_cast<uint8_t>(TelnetCommand::EOR):
    case static_cast<uint8_t>(TelnetCommand::SE):
    case static_cast<uint8_t>(TelnetCommand::NOP):
    case static_cast<uint8_t>(TelnetCommand::DM):
    case static_cast<uint8_t>(TelnetCommand::BRK):
    case static_cast<uint8_t>(TelnetCommand::IP):
    case static_cast<uint8_t>(TelnetCommand::AO):
    case static_cast<uint8_t>(TelnetCommand::AYT):
    case static_cast<uint8_t>(TelnetCommand::EC):
    case static_cast<uint8_t>(TelnetCommand::EL):
    case static_cast<uint8_t>(TelnetCommand::GA):
        return true;
    default:
        return false;
    }
}

constexpr bool isStandaloneTelnetCommand(TelnetCommand cmd) {
    return isStandaloneTelnetCommand(static_cast<uint8_t>(cmd));
}

std::string telnetCommandToString(TelnetCommand cmd);

} // namespace telnet
