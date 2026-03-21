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
#include <functional>
#include <vector>

#include "commands.h"
#include "options.h"

namespace telnet {

// Telnet stream parser and negotiator. Unpacks Telnet IAC sequences and
// surfaces application data (non-Telnet bytes) via a user-provided callback.
class Client {
  public:
    Client();

    // Set the callback to receive application data (Telnet-unescaped payload)
    void setAppDataCallback(std::function<void(const std::vector<uint8_t> &)> cb);

    // Feed incoming bytes from the network into the Telnet parser
    void feed(const std::vector<uint8_t> &data);

    // Reset Telnet parsing state
    void reset();

    // Callbacks for Telnet events (replaces Qt signals)
    std::function<void(telnet::TelnetCommand, telnet::TelnetOption)> onNegotiationCommand;
    std::function<void(telnet::TelnetOption, const std::vector<uint8_t> &)> onSubnegotiation;
    std::function<void(telnet::TelnetCommand)> onStandaloneCommand;

  private:
    enum class State {
        Data,
        IAC,
        NegotiationOption,
        SubnegotiationOption,
        SubnegotiationData,
        SubnegotiationIAC
    };

    void flushAppData();

    State m_state;
    std::vector<uint8_t> m_appBuffer;

    telnet::TelnetCommand m_pendingCmd;
    telnet::TelnetOption m_pendingOpt;

    telnet::TelnetOption m_sbOpt;
    std::vector<uint8_t> m_sbBuffer;

    std::function<void(const std::vector<uint8_t> &)> m_onAppData;
};

} // namespace telnet
