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

#include "codepage/codepage.h"
#include "telnet/commands.h"
#include "telnet/options.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tn5250::client {

class ProtocolHandler {
  public:
    enum class ConnectionState { Disconnected, Connecting, Negotiating, Connected, Error };

    struct Config {
        std::string deviceName;
        std::string terminalType = "IBM-3179-2";
        std::string username;
        std::string password;
        tn5250::codepage::CodePage::ID codePage = tn5250::codepage::CodePage::ID::CP037;
    };

    struct Callbacks {
        std::function<void(const std::vector<uint8_t> &)> onSendBytes;
        std::function<void()> onConnected;
        std::function<void()> onDisconnected;
        std::function<void(const std::vector<uint8_t> &)> onDataReceived;
        std::function<void(const std::string &)> onError;
        std::function<void(ConnectionState)> onStateChanged;
        std::function<void(const std::string &)> onLog;
    };

    explicit ProtocolHandler(const Config &config, const Callbacks &callbacks);

    // Call when TCP connection is established
    void onSocketConnected();
    // Call when raw bytes arrive from the network
    void onBytesReceived(const std::vector<uint8_t> &data);
    // Call when socket disconnects
    void onSocketDisconnected();

    // Send application data (wraps in GDS record)
    void sendData(const std::vector<uint8_t> &data);
    void sendGDS(uint8_t flagsHi, uint8_t opcode, const std::vector<uint8_t> &payload);

    // Returns IAC NOP bytes for heartbeat; caller manages timing
    std::vector<uint8_t> heartbeatPayload() const;

    ConnectionState state() const { return m_state; }
    bool isHandshakeComplete() const { return m_handshakeComplete; }

  private:
    void setState(ConnectionState newState);

    // Telnet
    void processTelnetData(const std::vector<uint8_t> &data);
    void sendTelnetCommand(telnet::TelnetCommand cmd, telnet::TelnetOption opt);
    void handleTelnetCommand(uint8_t cmd, uint8_t opt);
    void handleSubnegotiation(telnet::TelnetOption opt, const std::vector<uint8_t> &data);
    void sendRawData(const std::vector<uint8_t> &data);

    // Handshake
    void performHandshake();
    void sendDeviceName();
    void sendNewEnviron();
    void processHandshakeData(const std::vector<uint8_t> &data);
    void checkHandshakeComplete();

    Config m_config;
    Callbacks m_callbacks;
    ConnectionState m_state;

    std::vector<uint8_t> m_serverSeed;
    std::vector<uint8_t> m_receiveBuffer;

    bool m_inSubnegotiation;
    telnet::TelnetOption m_currentSubnegotiation;
    std::vector<uint8_t> m_subnegotiationBuffer;

    bool m_handshakeComplete;
    std::vector<uint8_t> m_handshakeBuffer;
    bool m_binaryNegotiated;
    bool m_eorNegotiated;
    bool m_terminalTypeSent;
};

} // namespace tn5250::client
