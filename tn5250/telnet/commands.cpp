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

#include "commands.h"

#include <cstdio>
#include <string>

namespace telnet {

std::string telnetCommandToString(telnet::TelnetCommand cmd) {
    using telnet::TelnetCommand;
    switch (cmd) {
    case TelnetCommand::EOR:  return "EOR";
    case TelnetCommand::SE:   return "SE";
    case TelnetCommand::NOP:  return "NOP";
    case TelnetCommand::DM:   return "DM";
    case TelnetCommand::BRK:  return "BRK";
    case TelnetCommand::IP:   return "IP";
    case TelnetCommand::AO:   return "AO";
    case TelnetCommand::AYT:  return "AYT";
    case TelnetCommand::EC:   return "EC";
    case TelnetCommand::EL:   return "EL";
    case TelnetCommand::GA:   return "GA";
    case TelnetCommand::SB:   return "SB";
    case TelnetCommand::WILL: return "WILL";
    case TelnetCommand::WONT: return "WONT";
    case TelnetCommand::DO:   return "DO";
    case TelnetCommand::DONT: return "DONT";
    case TelnetCommand::IAC:  return "IAC";
    default: {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "UNKNOWN(0x%02X)",
                      static_cast<unsigned>(static_cast<uint8_t>(cmd)));
        return buf;
    }
    }
}

constexpr bool isIAC(uint8_t byte) {
    return byte == static_cast<uint8_t>(TelnetCommand::IAC);
}

} // namespace telnet
